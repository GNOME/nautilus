/* nautilus-view.h
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
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
 * Authors: Ettore Perazzoli
 *             Darin Adler <darin@bentspoon.com>
 *             John Sullivan <sullivan@eazel.com>
 *          Pavel Cisler <pavel@eazel.com>
 */

#pragma once

#include <gtk/gtk.h>
#include <gio/gio.h>

#include "nautilus-directory.h"
#include "nautilus-file.h"

#include "nautilus-window.h"
#include "nautilus-view.h"
#include "nautilus-window-slot.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_FILES_VIEW nautilus_files_view_get_type()

G_DECLARE_DERIVABLE_TYPE (NautilusFilesView, nautilus_files_view, NAUTILUS, FILES_VIEW, GtkGrid)

struct _NautilusFilesViewClass {
        GtkGridClass parent_class;

        /* The 'clear' signal is emitted to empty the view of its contents.
         * It must be replaced by each subclass.
         */
        void         (* clear)                  (NautilusFilesView *view);

        /* The 'begin_file_changes' signal is emitted before a set of files
         * are added to the view. It can be replaced by a subclass to do any
         * necessary preparation for a set of new files. The default
         * implementation does nothing.
         */
        void         (* begin_file_changes)     (NautilusFilesView *view);

        /* The 'add_files' signal is emitted to add a set of files to the view.
         * It must be replaced by each subclass.
         */
        void    (* add_files)                    (NautilusFilesView *view,
                                                  GList             *files);
        void    (* remove_file)                 (NautilusFilesView *view,
                                                 NautilusFile      *file,
                                                 NautilusDirectory *directory);

        /* The 'file_changed' signal is emitted to signal a change in a file,
         * including the file being removed.
         * It must be replaced by each subclass.
         */
        void         (* file_changed)         (NautilusFilesView *view,
                                               NautilusFile      *file,
                                               NautilusDirectory *directory);

        /* The 'end_file_changes' signal is emitted after a set of files
         * are added to the view. It can be replaced by a subclass to do any
         * necessary cleanup (typically, cleanup for code in begin_file_changes).
         * The default implementation does nothing.
         */
        void         (* end_file_changes)    (NautilusFilesView *view);

        /* The 'begin_loading' signal is emitted before any of the contents
         * of a directory are added to the view. It can be replaced by a
         * subclass to do any necessary preparation to start dealing with a
         * new directory. The default implementation does nothing.
         */
        void         (* begin_loading)       (NautilusFilesView *view);

        /* The 'end_loading' signal is emitted after all of the contents
         * of a directory are added to the view. It can be replaced by a
         * subclass to do any necessary clean-up. The default implementation
         * does nothing.
         *
         * If all_files_seen is true, the handler may assume that
         * no load error ocurred, and all files of the underlying
         * directory were loaded.
         *
         * Otherwise, end_loading was emitted due to cancellation,
         * which usually means that not all files are available.
         */
        void         (* end_loading)          (NautilusFilesView *view,
                                               gboolean           all_files_seen);

        /* Function pointers that don't have corresponding signals */

        /* get_backing uri is a function pointer for subclasses to
         * override. Subclasses may replace it with a function that
         * returns the URI for the location where to create new folders,
         * files, links and paste the clipboard to.
         */

        char *        (* get_backing_uri)    (NautilusFilesView *view);

        /* get_selection is not a signal; it is just a function pointer for
         * subclasses to replace (override). Subclasses must replace it
         * with a function that returns a newly-allocated GList of
         * NautilusFile pointers.
         */
        GList *        (* get_selection)     (NautilusFilesView *view);

        /* get_selection_for_file_transfer  is a function pointer for
         * subclasses to replace (override). Subclasses must replace it
         * with a function that returns a newly-allocated GList of
         * NautilusFile pointers. The difference from get_selection is
         * that any files in the selection that also has a parent folder
         * in the selection is not included.
         */
        GList *        (* get_selection_for_file_transfer)(NautilusFilesView *view);

        /* select_all is a function pointer that subclasses must override to
         * select all of the items in the view */
        void     (* select_all)              (NautilusFilesView *view);

        /* select_first is a function pointer that subclasses must override to
         * select the first item in the view */
        void     (* select_first)            (NautilusFilesView *view);

        /* set_selection is a function pointer that subclasses must
         * override to select the specified items (and unselect all
         * others). The argument is a list of NautilusFiles. */

        void     (* set_selection)           (NautilusFilesView *view,
                                              GList             *selection);

        /* invert_selection is a function pointer that subclasses must
         * override to invert selection. */

        void     (* invert_selection)        (NautilusFilesView *view);

        /* bump_zoom_level is a function pointer that subclasses must override
         * to change the zoom level of an object. */
        void    (* bump_zoom_level)          (NautilusFilesView *view,
                                              int                zoom_increment);

        /*
         * restore_default_zoom_level: restores the zoom level to 100% (or to
         * whatever is considered the 'standard' zoom level for the view). */
        void    (* restore_standard_zoom_level) (NautilusFilesView *view);

        /* can_zoom_in is a function pointer that subclasses must override to
         * return whether the view is at maximum size (furthest-in zoom level) */
        gboolean (* can_zoom_in)             (NautilusFilesView *view);

        /* can_zoom_out is a function pointer that subclasses must override to
         * return whether the view is at minimum size (furthest-out zoom level) */
        gboolean (* can_zoom_out)            (NautilusFilesView *view);

        /* The current zoom level as a percentage of the default. */
        gfloat   (* get_zoom_level_percentage) (NautilusFilesView *view);

        gboolean (*is_zoom_level_default)      (NautilusFilesView *view);

        /* reveal_selection is a function pointer that subclasses may
         * override to make sure the selected items are sufficiently
         * apparent to the user (e.g., scrolled into view). By default,
         * this does nothing.
         */
        void     (* reveal_selection)        (NautilusFilesView *view);

        /* update_menus is a function pointer that subclasses can override to
         * update the sensitivity or wording of menu items in the menu bar.
         * It is called (at least) whenever the selection changes. If overridden,
         * subclasses must call parent class's function.
         */
        void    (* update_context_menus)     (NautilusFilesView *view);

        void    (* update_actions_state)     (NautilusFilesView *view);

        /* sort_files is a function pointer that subclasses can override
         * to provide a sorting order to determine which files should be
         * presented when only a partial list is provided.
         */
        int     (* compare_files)            (NautilusFilesView *view,
                                              NautilusFile      *a,
                                              NautilusFile      *b);

        /* is_empty is a function pointer that subclasses must
         * override to report whether the view contains any items.
         */
        gboolean (* is_empty)                (NautilusFilesView *view);

        /* Preference change callbacks, overridden by icon and list views.
         * Icon and list views respond by synchronizing to the new preference
         * values and forcing an update if appropriate.
         */
        void        (* click_policy_changed) (NautilusFilesView *view);
        void        (* sort_directories_first_changed) (NautilusFilesView *view);

        /* Get the id for this view. Its a guint*/
        guint        (* get_view_id)       (NautilusFilesView *view);

        /* Return the uri of the first visible file */
        char *         (* get_first_visible_file) (NautilusFilesView          *view);
        /* Scroll the view so that the file specified by the uri is at the top
           of the view */
        void           (* scroll_to_file)    (NautilusFilesView *view,
                                              const char        *uri);

        NautilusWindow * (*get_window)       (NautilusFilesView *view);

        GdkRectangle * (* compute_rename_popover_pointing_to) (NautilusFilesView *view);

        GdkRectangle * (* reveal_for_selection_context_menu) (NautilusFilesView *view);

        /* Use this to show an optional visual feedback when the directory is empty.
         * By default it shows a widget overlay on top of the view */
        void           (* check_empty_states)          (NautilusFilesView *view);

        void           (* preview_selection_event)     (NautilusFilesView *view,
                                                        GtkDirectionType   direction);
};

NautilusFilesView *      nautilus_files_view_new                         (guint               id,
                                                                          NautilusWindowSlot *slot);

/* Functions callable from the user interface and elsewhere. */
NautilusWindowSlot *nautilus_files_view_get_nautilus_window_slot         (NautilusFilesView *view);
char *              nautilus_files_view_get_uri                          (NautilusFilesView *view);

void                nautilus_files_view_display_selection_info           (NautilusFilesView *view);

/* Wrappers for signal emitters. These are normally called
 * only by NautilusFilesView itself. They have corresponding signals
 * that observers might want to connect with.
 */
gboolean            nautilus_files_view_get_loading                      (NautilusFilesView *view);

/* Hooks for subclasses to call. These are normally called only by
 * NautilusFilesView and its subclasses
 */
void                nautilus_files_view_activate_files                   (NautilusFilesView *view,
                                                                          GList             *files,
                                                                          NautilusOpenFlags  flags,
                                                                          gboolean           confirm_multiple);
void                nautilus_files_view_activate_file                    (NautilusFilesView *view,
                                                                          NautilusFile      *file,
                                                                          NautilusOpenFlags  flags);
void                nautilus_files_view_start_batching_selection_changes (NautilusFilesView *view);
void                nautilus_files_view_stop_batching_selection_changes  (NautilusFilesView *view);
void                nautilus_files_view_notify_selection_changed         (NautilusFilesView *view);
NautilusDirectory  *nautilus_files_view_get_model                        (NautilusFilesView *view);
NautilusFile       *nautilus_files_view_get_directory_as_file            (NautilusFilesView *view);
void                nautilus_files_view_pop_up_background_context_menu   (NautilusFilesView *view,
                                                                          gdouble            x,
                                                                          gdouble            y);
void                nautilus_files_view_pop_up_selection_context_menu    (NautilusFilesView *view,
                                                                          gdouble            x,
                                                                          gdouble            y);
gboolean            nautilus_files_view_should_show_file                 (NautilusFilesView *view,
                                                                          NautilusFile      *file);
gboolean            nautilus_files_view_should_sort_directories_first    (NautilusFilesView *view);
void                nautilus_files_view_ignore_hidden_file_preferences   (NautilusFilesView *view);

void                nautilus_files_view_add_subdirectory                (NautilusFilesView *view,
                                                                         NautilusDirectory *directory);
void                nautilus_files_view_remove_subdirectory             (NautilusFilesView *view,
                                                                         NautilusDirectory *directory);

gboolean            nautilus_files_view_is_editable              (NautilusFilesView      *view);
NautilusWindow *    nautilus_files_view_get_window               (NautilusFilesView      *view);

/* file operations */
char *            nautilus_files_view_get_backing_uri            (NautilusFilesView      *view);
void              nautilus_files_view_move_copy_items            (NautilusFilesView      *view,
                                                                  const GList            *item_uris,
                                                                  const char             *target_uri,
                                                                  int                     copy_action);
void              nautilus_files_view_new_file_with_initial_contents (NautilusFilesView  *view,
                                                                      const char         *parent_uri,
                                                                      const char         *filename,
                                                                      const char         *initial_contents,
                                                                      int                 length);

/* clipboard reading */
void               nautilus_files_view_get_clipboard_async  (NautilusFilesView   *self,
                                                             GAsyncReadyCallback  callback,
                                                             gpointer             callback_data);
NautilusClipboard *nautilus_files_view_get_clipboard_finish (NautilusFilesView  *self,
                                                             GAsyncResult       *result,
                                                             GError            **error);

/* selection handling */
void              nautilus_files_view_activate_selection         (NautilusFilesView      *view);
void              nautilus_files_view_preview_selection_event    (NautilusFilesView      *view,
                                                                  GtkDirectionType        direction);
void              nautilus_files_view_stop_loading               (NautilusFilesView      *view);

char *            nautilus_files_view_get_first_visible_file     (NautilusFilesView      *view);
void              nautilus_files_view_scroll_to_file             (NautilusFilesView      *view,
                                                                  const char             *uri);
char *            nautilus_files_view_get_title                  (NautilusFilesView      *view);
gboolean          nautilus_files_view_supports_zooming           (NautilusFilesView      *view);
void              nautilus_files_view_bump_zoom_level            (NautilusFilesView      *view,
                                                                  int                     zoom_increment);
gboolean          nautilus_files_view_can_zoom_in                (NautilusFilesView      *view);
gboolean          nautilus_files_view_can_zoom_out               (NautilusFilesView      *view);

void              nautilus_files_view_update_context_menus       (NautilusFilesView      *view);
void              nautilus_files_view_update_toolbar_menus       (NautilusFilesView      *view);
void              nautilus_files_view_update_actions_state       (NautilusFilesView      *view);

void              nautilus_files_view_action_show_hidden_files   (NautilusFilesView      *view,
                                                                  gboolean                show_hidden);

GActionGroup *    nautilus_files_view_get_action_group           (NautilusFilesView      *view);
GtkWidget*        nautilus_files_view_get_content_widget         (NautilusFilesView      *view);

G_END_DECLS
