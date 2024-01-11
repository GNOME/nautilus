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

G_DECLARE_DERIVABLE_TYPE (NautilusFilesView, nautilus_files_view, NAUTILUS, FILES_VIEW, AdwBin)

struct _NautilusFilesViewClass {
        AdwBinClass parent_class;

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
         */
        void    (* add_files)                    (NautilusFilesView *view,
                                                  GList             *files);
        void    (* remove_files)                 (NautilusFilesView *view,
                                                 GList             *files,
                                                 NautilusDirectory *directory);

        /* The 'file_changed' signal is emitted to signal a change in a file,
         * including the file being removed.
         */
        void         (* file_changed)         (NautilusFilesView *view,
                                               NautilusFile      *file,
                                               NautilusDirectory *directory);

        /* The 'end_file_changes' signal is emitted after a set of files
         * are added to the view. It can be connected to in order to do any
         * necessary cleanup (typically, cleanup for code in begin_file_changes).
         */
        void         (* end_file_changes)    (NautilusFilesView *view);

        /* The 'begin_loading' signal is emitted before any of the contents
         * of a directory are added to the view. It can be replaced by a
         * subclass to do any necessary preparation to start dealing with a
         * new directory. The default implementation does nothing.
         */
        void         (* begin_loading)       (NautilusFilesView *view);

        /* The 'end_loading' signal is emitted after all of the contents
         * of a directory are added to the view.
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

        /* update_menus is a function pointer that subclasses can override to
         * update the sensitivity or wording of menu items in the menu bar.
         * It is called (at least) whenever the selection changes. If overridden,
         * subclasses must call parent class's function.
         */
        void    (* update_context_menus)     (NautilusFilesView *view);

        void    (* update_actions_state)     (NautilusFilesView *view);

        GdkRectangle * (* reveal_for_selection_context_menu) (NautilusFilesView *view);

        /* Use this to show an optional visual feedback when the directory is empty.
         * By default it shows a widget overlay on top of the view */
        void           (* check_empty_states)          (NautilusFilesView *view);
};

NautilusFilesView *      nautilus_files_view_new                         (guint               id,
                                                                          NautilusWindowSlot *slot);

/* Functions callable from the user interface and elsewhere. */
NautilusWindowSlot *nautilus_files_view_get_nautilus_window_slot         (NautilusFilesView *view);

/* Wrappers for signal emitters. These are normally called
 * only by NautilusFilesView itself. They have corresponding signals
 * that observers might want to connect with.
 */
gboolean            nautilus_files_view_get_loading                      (NautilusFilesView *view);

/* Hooks for subclasses to call. These are normally called only by
 * NautilusFilesView and its subclasses
 */
void                nautilus_files_view_activate_file                    (NautilusFilesView *view,
                                                                          NautilusFile      *file,
                                                                          NautilusOpenFlags  flags);
void                nautilus_files_view_pop_up_background_context_menu   (NautilusFilesView *view,
                                                                          gdouble            x,
                                                                          gdouble            y);
void                nautilus_files_view_pop_up_selection_context_menu    (NautilusFilesView *view,
                                                                          gdouble            x,
                                                                          gdouble            y);

gboolean            nautilus_files_view_has_subdirectory                (NautilusFilesView *view,
                                                                         NautilusDirectory *directory);
void                nautilus_files_view_add_subdirectory                (NautilusFilesView *view,
                                                                         NautilusDirectory *directory);
void                nautilus_files_view_remove_subdirectory             (NautilusFilesView *view,
                                                                         NautilusDirectory *directory);
gpointer            nautilus_files_view_get_model                       (NautilusFilesView *view);

/* file operations */
char *            nautilus_files_view_get_backing_uri            (NautilusFilesView      *view);
void              nautilus_files_view_move_copy_items            (NautilusFilesView      *view,
                                                                  const GList            *item_uris,
                                                                  const char             *target_uri,
                                                                  int                     copy_action);
void              nautilus_file_view_save_image_from_texture    (NautilusFilesView       *view,
                                                                 GdkTexture              *texture,
                                                                 const char              *target_uri,
                                                                 const char              *base_name);
void              nautilus_files_view_new_file_with_initial_contents (NautilusFilesView  *view,
                                                                      const char         *parent_uri,
                                                                      const char         *filename,
                                                                      const char         *initial_contents,
                                                                      gsize               length);
/* selection handling */
void              nautilus_files_view_activate_selection         (NautilusFilesView      *view,
                                                                  NautilusOpenFlags       flags);
void              nautilus_files_view_preview_selection_event    (NautilusFilesView      *view,
                                                                  GtkDirectionType        direction);
void              nautilus_files_view_stop_loading               (NautilusFilesView      *view);

void              nautilus_files_view_update_context_menus       (NautilusFilesView      *view);
void              nautilus_files_view_update_toolbar_menus       (NautilusFilesView      *view);
void              nautilus_files_view_update_actions_state       (NautilusFilesView      *view);

GtkWidget*        nautilus_files_view_get_content_widget         (NautilusFilesView      *view);

G_END_DECLS
