#ifndef NAUTILUS_BATCH_RENAME_UTILITIES_H
#define NAUTILUS_BATCH_RENAME_UTILITIES_H

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
                                                 SortingMode                  mode,
                                                 GHashTable                  *creation_date_table);

gint compare_files_by_last_modified             (gconstpointer a,
                                                 gconstpointer b);

gint compare_files_by_first_modified            (gconstpointer a,
                                                 gconstpointer b);

gint compare_files_by_name_descending           (gconstpointer a,
                                                 gconstpointer b);

gint compare_files_by_name_ascending            (gconstpointer a,
                                                 gconstpointer b);

gint compare_files_by_first_created             (gconstpointer a,
                                                 gconstpointer b);

gint compare_files_by_last_created              (gconstpointer a,
                                                 gconstpointer b);

void check_metadata_for_selection               (NautilusBatchRenameDialog *dialog,
                                                 GList                     *selection);

gboolean selection_has_single_parent            (GList *selection);

void string_free                                (gpointer mem);

void conflict_data_free                         (gpointer mem);

GList* distinct_file_parents                    (GList *selection);

gboolean file_name_conflicts_with_results       (GList        *selection,
                                                 GList        *new_names,
                                                 GString      *old_name,
                                                 gchar        *parent_uri);

GString* batch_rename_dialog_replace_label_text        (gchar             *string,
                                                        const gchar       *substr);

#endif /* NAUTILUS_BATCH_RENAME_UTILITIES_H */