/*
 * SPDX-FileCopyrightText: 2016 Alexandru Pandelea <alexandru.pandelea@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <gio/gio.h>
#include <gtk/gtk.h>

GList* batch_rename_dialog_get_new_names_list          (NautilusBatchRenameDialogMode  mode,
                                                        GList                         *selection,
                                                        GList                         *tags_list,
                                                        GHashTable                    *selection_metadata,
                                                        gchar                         *entry_text,
                                                        gchar                         *replace_text);

GList* nautilus_batch_rename_dialog_sort        (GList                       *selection,
                                                 SortMode                     mode,
                                                 GHashTable                  *creation_date_table);

void check_metadata_for_selection               (NautilusBatchRenameDialog *dialog,
                                                 GList                     *selection,
                                                 GCancellable              *cancellable);

void string_free                                (gpointer mem);

void conflict_data_free                         (gpointer mem);

GList* batch_rename_files_get_distinct_parents  (GList *selection);

gboolean file_name_conflicts_with_results       (GList        *selection,
                                                 GList        *new_names,
                                                 GString      *old_name,
                                                 gchar        *parent_uri);

GString* markup_hightlight_text                 (const char  *label,
                                                 const gchar *substring,
                                                 const gchar *replacement_text,
                                                 const gchar *text_color,
                                                 const gchar *background_color);

const gchar* batch_rename_get_tag_text_representation (TagConstants tag_constants);
