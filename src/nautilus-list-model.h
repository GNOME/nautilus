
/* fm-list-model.h - a GtkTreeModel for file lists. 

   Copyright (C) 2001, 2002 Anders Carlsson

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Authors: Anders Carlsson <andersca@gnu.org>
*/

#pragma once

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "nautilus-file.h"
#include "nautilus-directory.h"
#include <nautilus-extension.h>

#define NAUTILUS_TYPE_LIST_MODEL nautilus_list_model_get_type()
G_DECLARE_DERIVABLE_TYPE (NautilusListModel, nautilus_list_model, NAUTILUS, LIST_MODEL, GObject);

enum {
	NAUTILUS_LIST_MODEL_FILE_COLUMN,
	NAUTILUS_LIST_MODEL_SUBDIRECTORY_COLUMN,
	NAUTILUS_LIST_MODEL_SMALL_ICON_COLUMN,
	NAUTILUS_LIST_MODEL_STANDARD_ICON_COLUMN,
	NAUTILUS_LIST_MODEL_LARGE_ICON_COLUMN,
	NAUTILUS_LIST_MODEL_LARGER_ICON_COLUMN,
	NAUTILUS_LIST_MODEL_FILE_NAME_IS_EDITABLE_COLUMN,
	NAUTILUS_LIST_MODEL_NUM_COLUMNS
};

struct _NautilusListModelClass
{
	GObjectClass parent_class;

	void (* subdirectory_unloaded)(NautilusListModel *model,
				       NautilusDirectory *subdirectory);
};

gboolean nautilus_list_model_add_file                          (NautilusListModel          *model,
								NautilusFile         *file,
								NautilusDirectory    *directory);
void     nautilus_list_model_file_changed                      (NautilusListModel          *model,
								NautilusFile         *file,
								NautilusDirectory    *directory);
gboolean nautilus_list_model_is_empty                          (NautilusListModel          *model);
void     nautilus_list_model_remove_file                       (NautilusListModel          *model,
								NautilusFile         *file,
								NautilusDirectory    *directory);
void     nautilus_list_model_clear                             (NautilusListModel          *model);
gboolean nautilus_list_model_get_tree_iter_from_file           (NautilusListModel          *model,
								NautilusFile         *file,
								NautilusDirectory    *directory,
								GtkTreeIter          *iter);
GList *  nautilus_list_model_get_all_iters_for_file            (NautilusListModel          *model,
								NautilusFile         *file);
gboolean nautilus_list_model_get_first_iter_for_file           (NautilusListModel          *model,
								NautilusFile         *file,
								GtkTreeIter          *iter);
void     nautilus_list_model_set_should_sort_directories_first (NautilusListModel          *model,
								gboolean              sort_directories_first);

int      nautilus_list_model_get_sort_column_id_from_attribute (NautilusListModel *model,
								GQuark       attribute);
GQuark   nautilus_list_model_get_attribute_from_sort_column_id (NautilusListModel *model,
								int sort_column_id);
void     nautilus_list_model_sort_files                        (NautilusListModel *model,
								GList **files);

NautilusListZoomLevel nautilus_list_model_get_zoom_level_from_column_id (int               column);
int               nautilus_list_model_get_column_id_from_zoom_level (NautilusListZoomLevel zoom_level);
guint    nautilus_list_model_get_icon_size_for_zoom_level      (NautilusListZoomLevel zoom_level);

NautilusFile *    nautilus_list_model_file_for_path (NautilusListModel *model, GtkTreePath *path);
gboolean          nautilus_list_model_load_subdirectory (NautilusListModel *model, GtkTreePath *path, NautilusDirectory **directory);
void              nautilus_list_model_unload_subdirectory (NautilusListModel *model, GtkTreeIter *iter);

void              nautilus_list_model_set_drag_view (NautilusListModel *model,
						     GtkTreeView *view,
						     int begin_x, 
						     int begin_y);
GtkTreeView *     nautilus_list_model_get_drag_view (NautilusListModel *model,
						     int *drag_begin_x,
						     int *drag_begin_y);

GtkTargetList *   nautilus_list_model_get_drag_target_list (void);

int               nautilus_list_model_compare_func (NautilusListModel *model,
						    NautilusFile *file1,
						    NautilusFile *file2);


int               nautilus_list_model_add_column (NautilusListModel *model,
						  NautilusColumn *column);

void              nautilus_list_model_subdirectory_done_loading (NautilusListModel       *model,
								 NautilusDirectory *directory);

void              nautilus_list_model_set_highlight_for_files (NautilusListModel *model,
							       GList *files);