/* nautilus-list-view-private.h
 *
 * Copyright (C) 2015 Carlos Soriano <csoriano@gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Data and functions shared between list view and list view dnd */

#pragma once

#include "nautilus-list-model.h"
#include "nautilus-tree-view-drag-dest.h"
#include "nautilus-dnd.h"
#include "nautilus-tag-manager.h"

struct NautilusListViewDetails {
  GtkTreeView *tree_view;
  NautilusListModel *model;

  GtkTreeViewColumn   *file_name_column;
  int file_name_column_num;

  GtkCellRendererPixbuf *pixbuf_cell;
  GtkCellRendererText   *file_name_cell;
  GList *cells;

  NautilusListZoomLevel zoom_level;

  NautilusTreeViewDragDest *drag_dest;

  GtkTreePath *first_click_path; /* Both clicks in a double click need to be on the same row */

  GtkTreePath *new_selection_path;   /* Path of the new selection after removing a file */

  GtkTreePath *hover_path;

  gint last_event_button_x;
  gint last_event_button_y;

  guint drag_button;
  int drag_x;
  int drag_y;

  gboolean drag_started;
  gboolean ignore_button_release;
  gboolean row_selected_on_button_down;
  gboolean active;
  NautilusDragInfo *drag_source_info;

  GHashTable *columns;
  GtkWidget *column_editor;

  char *original_name;

  gulong clipboard_handler_id;

  GQuark last_sort_attr;

  GRegex *regex;

  NautilusTagManager *tag_manager;
  GCancellable *starred_cancellable;

  GtkGesture *tree_view_drag_gesture;
  GtkGesture *tree_view_multi_press_gesture;
  GtkEventController *motion_controller;
  GtkEventController *key_controller;
};

