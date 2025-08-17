/* nautilus-files-view.h
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

#include "nautilus-types.h"

#include <adwaita.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_FILES_VIEW nautilus_files_view_get_type()
G_DECLARE_FINAL_TYPE (NautilusFilesView, nautilus_files_view, NAUTILUS, FILES_VIEW, AdwBin)

NautilusFilesView *      nautilus_files_view_new                         (guint               id,
                                                                          NautilusWindowSlot *slot);

guint
nautilus_files_view_get_view_id (NautilusFilesView *self);
void                nautilus_files_view_change                           (NautilusFilesView  *self,
                                                                          guint               id);

const char *
nautilus_files_view_get_toggle_icon_name (NautilusFilesView *self);
const char *
nautilus_files_view_get_toggle_tooltip (NautilusFilesView *self);

GFile *
nautilus_files_view_get_location (NautilusFilesView *self);
void
nautilus_files_view_set_location (NautilusFilesView *self,
                                  GFile             *location);

NautilusQuery *
nautilus_files_view_get_search_query (NautilusFilesView *self);
void
nautilus_files_view_set_search_query (NautilusFilesView *self,
                                      NautilusQuery     *query);

NautilusFileList *
nautilus_files_view_get_selection (NautilusFilesView *self);
void
nautilus_files_view_set_selection (NautilusFilesView *self,
                                   NautilusFileList  *selection);

gboolean
nautilus_files_view_is_loading (NautilusFilesView *self);
gboolean
nautilus_files_view_is_searching (NautilusFilesView *self);

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

gboolean            nautilus_files_view_has_subdirectory                (NautilusFilesView *view,
                                                                         NautilusDirectory *directory);
void                nautilus_files_view_add_subdirectory                (NautilusFilesView *view,
                                                                         NautilusDirectory *directory);
void                nautilus_files_view_remove_subdirectory             (NautilusFilesView *view,
                                                                         NautilusDirectory *directory);

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
                                                                      const void         *initial_contents,
                                                                      gsize               length);
/* selection handling */
void              nautilus_files_view_activate_selection         (NautilusFilesView      *view,
                                                                  NautilusOpenFlags       flags);
void              nautilus_files_view_preview_selection_event    (NautilusFilesView      *view,
                                                                  GtkDirectionType        direction);
void              nautilus_files_view_stop_loading               (NautilusFilesView      *view);

NautilusToolbarMenuSections *
nautilus_files_view_get_toolbar_menu_sections (NautilusFilesView *self);

void              nautilus_files_view_update_context_menus       (NautilusFilesView      *view);
void              nautilus_files_view_update_toolbar_menus       (NautilusFilesView      *view);
void              nautilus_files_view_update_actions_state       (NautilusFilesView      *view);

/* testing-only */
NautilusViewModel *
nautilus_files_view_get_private_model (NautilusFilesView *self);

G_END_DECLS
