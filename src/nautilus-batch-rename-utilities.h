/* nautilus-batch-rename-utilities.c
 *
 * Copyright (C) 2016 Alexandru Pandelea <alexandru.pandelea@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
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

#pragma once

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <tracker-sparql.h>

GList* batch_rename_dialog_get_new_names_list          (NautilusBatchRenameDialogMode  mode,
                                                        GList                         *selection,
                                                        GList                         *tags_list,
                                                        GList                         *selection_metadata,
                                                        gchar                         *entry_text,
                                                        gchar                         *replace_text);

GList* file_names_list_has_duplicates                      (NautilusBatchRenameDialog   *dialog,
                                                            NautilusDirectory           *model,
                                                            GList                       *names,
                                                            GList                       *selection,
                                                            GList                       *parents_list,
                                                            GCancellable                *cancellable);

GList* nautilus_batch_rename_dialog_sort        (GList                       *selection,
                                                 SortMode                     mode,
                                                 GHashTable                  *creation_date_table);

void check_metadata_for_selection               (NautilusBatchRenameDialog *dialog,
                                                 GList                     *selection,
                                                 GCancellable              *cancellable);

gboolean selection_has_single_parent            (GList *selection);

void string_free                                (gpointer mem);

void conflict_data_free                         (gpointer mem);

GList* batch_rename_files_get_distinct_parents  (GList *selection);

gboolean file_name_conflicts_with_results       (GList        *selection,
                                                 GList        *new_names,
                                                 GString      *old_name,
                                                 gchar        *parent_uri);

GString* batch_rename_replace_label_text        (const char        *label,
                                                 const gchar       *substr);

gchar*   batch_rename_get_tag_text_representation (TagConstants tag_constants);

void batch_rename_sort_lists_for_rename (GList    **selection,
                                         GList    **new_names,
                                         GList    **old_names,
                                         GList    **new_files,
                                         GList    **old_files,
                                         gboolean   is_undo_redo);