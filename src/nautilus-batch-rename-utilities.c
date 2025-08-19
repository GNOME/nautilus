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

#include "nautilus-batch-rename-dialog.h"
#include "nautilus-batch-rename-utilities.h"
#include "nautilus-directory.h"
#include "nautilus-file.h"
#include "nautilus-filename-utilities.h"
#include "nautilus-localsearch-utilities.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdarg.h>

typedef struct
{
    NautilusFile *file;
    gint position;
} CreateDateElem;

typedef struct
{
    NautilusBatchRenameDialog *dialog;
    GHashTable *date_order_hash_table;

    GHashTable *selection_metadata;

    gboolean has_metadata[G_N_ELEMENTS (metadata_tags_constants)];

    GCancellable *cancellable;
} QueryData;

enum
{
    FILE_NAME_INDEX,
    CREATION_DATE_INDEX,
    CAMERA_MODEL_INDEX,
    SEASON_INDEX,
    EPISODE_NUMBER_INDEX,
    TRACK_NUMBER_INDEX,
    ARTIST_NAME_INDEX,
    TITLE_INDEX,
    ALBUM_NAME_INDEX,
    URL_INDEX,
} QueryMetadata;

static void on_cursor_callback (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data);

void
string_free (gpointer mem)
{
    if (mem != NULL)
    {
        g_string_free (mem, TRUE);
    }
}

void
conflict_data_free (gpointer mem)
{
    ConflictData *conflict_data = mem;

    g_free (conflict_data->name);
    g_free (conflict_data);
}

static void
file_metadata_free (gpointer data)
{
    FileMetadata *file_metadata = data;

    for (guint i = 0; i < G_N_ELEMENTS (file_metadata->metadata); i++)
    {
        if (file_metadata->metadata[i] != NULL)
        {
            g_string_free (file_metadata->metadata[i], TRUE);
        }
    }

    g_free (file_metadata);
}

const gchar *
batch_rename_get_tag_text_representation (TagConstants tag_constants)
{
    static GHashTable *tag_text_hash;

    if (G_UNLIKELY (tag_text_hash == NULL))
    {
        tag_text_hash = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
    }

    const gchar *tag_text = g_hash_table_lookup (tag_text_hash, tag_constants.label);
    if (G_UNLIKELY (tag_text == NULL))
    {
        tag_text = g_strdup_printf ("[%s]", gettext (tag_constants.label));

        g_hash_table_insert (tag_text_hash,
                             (gpointer) tag_constants.label,
                             (gpointer) tag_text);
    }

    return tag_text;
}

void
batch_rename_sort_lists_for_rename (GList    **selection,
                                    GList    **new_names,
                                    GList    **old_names,
                                    GList    **new_files,
                                    GList    **old_files,
                                    gboolean   is_undo_redo)
{
    GList *new_names_list;
    GList *new_names_list2;
    GList *files;
    GList *files2;
    GList *old_names_list = NULL;
    GList *new_files_list = NULL;
    GList *old_files_list = NULL;
    GList *old_names_list2 = NULL;
    GList *new_files_list2 = NULL;
    GList *old_files_list2 = NULL;
    GString *new_file_name;
    GString *new_name;
    GString *old_name;
    GFile *new_file;
    GFile *old_file;
    NautilusFile *file;
    gboolean order_changed = TRUE;

    /* in the following case:
     * file1 -> file2
     * file2 -> file3
     * file2 must be renamed first, so because of that, the list has to be reordered
     */
    while (order_changed)
    {
        order_changed = FALSE;

        if (is_undo_redo)
        {
            old_names_list = *old_names;
            new_files_list = *new_files;
            old_files_list = *old_files;
        }

        for (new_names_list = *new_names, files = *selection;
             new_names_list != NULL && files != NULL;
             new_names_list = new_names_list->next, files = files->next)
        {
            g_autoptr (NautilusFile) parent = NULL;

            new_file_name = new_names_list->data;
            parent = nautilus_file_get_parent (NAUTILUS_FILE (files->data));

            if (is_undo_redo)
            {
                old_names_list2 = old_names_list;
                new_files_list2 = new_files_list;
                old_files_list2 = old_files_list;
            }

            for (files2 = files, new_names_list2 = new_names_list;
                 files2 != NULL && new_names_list2 != NULL;
                 files2 = files2->next, new_names_list2 = new_names_list2->next)
            {
                const char *file_name;
                g_autoptr (NautilusFile) parent2 = NULL;

                file_name = nautilus_file_get_name (NAUTILUS_FILE (files2->data));
                new_name = new_names_list2->data;

                parent2 = nautilus_file_get_parent (NAUTILUS_FILE (files2->data));

                if (files2 != files && g_strcmp0 (file_name, new_file_name->str) == 0 &&
                    parent == parent2)
                {
                    file = NAUTILUS_FILE (files2->data);

                    *selection = g_list_remove_link (*selection, files2);
                    *new_names = g_list_remove_link (*new_names, new_names_list2);

                    *selection = g_list_prepend (*selection, file);
                    *new_names = g_list_prepend (*new_names, new_name);

                    if (is_undo_redo)
                    {
                        old_name = old_names_list2->data;
                        new_file = new_files_list2->data;
                        old_file = old_files_list2->data;

                        *old_names = g_list_remove_link (*old_names, old_names_list2);
                        *new_files = g_list_remove_link (*new_files, new_files_list2);
                        *old_files = g_list_remove_link (*old_files, old_files_list2);

                        *old_names = g_list_prepend (*old_names, old_name);
                        *new_files = g_list_prepend (*new_files, new_file);
                        *old_files = g_list_prepend (*old_files, old_file);
                    }

                    order_changed = TRUE;
                    break;
                }

                if (is_undo_redo)
                {
                    old_names_list2 = old_names_list2->next;
                    new_files_list2 = new_files_list2->next;
                    old_files_list2 = old_files_list2->next;
                }
            }

            if (is_undo_redo)
            {
                old_names_list = old_names_list->next;
                new_files_list = new_files_list->next;
                old_files_list = old_files_list->next;
            }
        }
    }
}

GString *
markup_hightlight_text (const char  *label,
                        const gchar *substring,
                        const gchar *text_color,
                        const gchar *background_color)
{
    g_autoptr (GString) new_label = g_string_new_take (g_markup_escape_text (label, -1));

    if (substring == NULL || *substring == '\0')
    {
        return g_steal_pointer (&new_label);
    }

    g_autofree gchar *escaped_substring = g_markup_escape_text (substring, -1);
    g_autofree gchar *highlighted_string = g_strdup_printf ("<span background='%s' color='%s'>%s</span>",
                                                            background_color, text_color,
                                                            escaped_substring);

    g_string_replace (new_label, escaped_substring, highlighted_string, -1);

    return g_steal_pointer (&new_label);
}

static gchar *
get_metadata (GHashTable   *selection_metadata,
              NautilusFile *file,
              MetadataType  metadata_type)
{
    if (selection_metadata == NULL)
    {
        /* We did not start collecting metadata yet */
        return NULL;
    }

    FileMetadata *file_metadata = g_hash_table_lookup (selection_metadata,
                                                       file);
    gchar *metadata = NULL;

    if (file_metadata != NULL &&
        file_metadata->metadata[metadata_type] != NULL &&
        file_metadata->metadata[metadata_type]->len > 0)
    {
        metadata = file_metadata->metadata[metadata_type]->str;
    }

    return metadata;
}

static GString *
batch_rename_format (NautilusFile *file,
                     GList        *text_chunks,
                     GHashTable   *selection_metadata,
                     gint          count)
{
    GList *l;
    GString *tag_string;
    GString *new_name;
    gboolean added_tag;
    MetadataType metadata_type;
    const char *file_name;
    gchar *metadata;

    file_name = nautilus_file_get_display_name (file);
    new_name = g_string_new ("");

    for (l = text_chunks; l != NULL; l = l->next)
    {
        added_tag = FALSE;
        tag_string = l->data;

        for (guint i = 0; i < G_N_ELEMENTS (numbering_tags_constants); i++)
        {
            const gchar *tag_text_representation;

            tag_text_representation = batch_rename_get_tag_text_representation (numbering_tags_constants[i]);
            if (g_strcmp0 (tag_string->str, tag_text_representation) == 0)
            {
                switch (numbering_tags_constants[i].numbering_type)
                {
                    case NUMBERING_NO_ZERO_PAD:
                    {
                        g_string_append_printf (new_name, "%d", count);
                    }
                    break;

                    case NUMBERING_ONE_ZERO_PAD:
                    {
                        g_string_append_printf (new_name, "%02d", count);
                    }
                    break;

                    case NUMBERING_TWO_ZERO_PAD:
                    {
                        g_string_append_printf (new_name, "%03d", count);
                    }
                    break;

                    default:
                    {
                        g_warn_if_reached ();
                    }
                    break;
                }

                added_tag = TRUE;
                break;
            }
        }

        if (added_tag)
        {
            continue;
        }

        for (guint i = 0; i < G_N_ELEMENTS (metadata_tags_constants); i++)
        {
            const gchar *tag_text_representation;

            tag_text_representation = batch_rename_get_tag_text_representation (metadata_tags_constants[i]);
            if (g_strcmp0 (tag_string->str, tag_text_representation) == 0)
            {
                metadata_type = metadata_tags_constants[i].metadata_type;
                metadata = get_metadata (selection_metadata, file, metadata_type);

                /* TODO: This is a hack, we should provide a cancellable for checking
                 * the metadata, and if that is happening don't enter here. We can
                 * special case original file name upper in the call stack */
                if (!metadata && metadata_type != ORIGINAL_FILE_NAME)
                {
                    g_warning ("Metadata not present in one file, it shouldn't have been added. File name: %s, Metadata: %s",
                               file_name, metadata_tags_constants[i].label);
                    continue;
                }

                switch (metadata_type)
                {
                    case ORIGINAL_FILE_NAME:
                    {
                        if (nautilus_file_is_directory (file))
                        {
                            new_name = g_string_append (new_name, file_name);
                        }
                        else
                        {
                            const char *extension = nautilus_filename_get_extension (file_name);
                            new_name = g_string_append_len (new_name, file_name, extension - file_name);
                        }
                    }
                    break;

                    case TRACK_NUMBER:
                    {
                        g_string_append_printf (new_name, "%02d", atoi (metadata));
                    }
                    break;

                    default:
                    {
                        new_name = g_string_append (new_name, metadata);
                    }
                    break;
                }

                added_tag = TRUE;
                break;
            }
        }

        if (!added_tag)
        {
            new_name = g_string_append (new_name, tag_string->str);
        }
    }

    if (g_strcmp0 (new_name->str, "") == 0)
    {
        new_name = g_string_append (new_name, file_name);
    }
    else if (!nautilus_file_is_directory (file))
    {
        const char *name = nautilus_file_get_name (file);
        if (name != NULL)
        {
            const char *extension = nautilus_filename_get_extension (name);
            new_name = g_string_append (new_name, extension);
        }
    }

    return new_name;
}

GList *
batch_rename_dialog_get_new_names_list (NautilusBatchRenameDialogMode  mode,
                                        GList                         *selection,
                                        GList                         *text_chunks,
                                        GHashTable                    *selection_metadata,
                                        gchar                         *entry_text,
                                        gchar                         *replace_text)
{
    GList *l;
    GList *result;
    GString *new_name;
    gint count;

    result = NULL;
    count = 1;

    for (l = selection; l != NULL; l = l->next)
    {
        NautilusFile *file = NAUTILUS_FILE (l->data);

        /* get the new name here and add it to the list*/
        if (mode == NAUTILUS_BATCH_RENAME_DIALOG_FORMAT)
        {
            new_name = batch_rename_format (file,
                                            text_chunks,
                                            selection_metadata,
                                            count++);
            result = g_list_prepend (result, new_name);
        }

        if (mode == NAUTILUS_BATCH_RENAME_DIALOG_REPLACE)
        {
            new_name = g_string_new (nautilus_file_get_name (file));

            g_string_replace (new_name, entry_text, replace_text, -1);

            result = g_list_prepend (result, new_name);
        }
    }

    return result;
}

/* There is a case that a new name for a file conflicts with an existing file name
 * in the directory but it's not a problem because the file in the directory that
 * conflicts is part of the batch renaming selection and it's going to change the name anyway. */
gboolean
file_name_conflicts_with_results (GList   *selection,
                                  GList   *new_names,
                                  GString *old_name,
                                  gchar   *parent_uri)
{
    GList *l1;
    GList *l2;
    GString *new_name;

    for (l1 = selection, l2 = new_names; l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next)
    {
        NautilusFile *selection_file = NAUTILUS_FILE (l1->data);
        const char *name1 = nautilus_file_get_name (selection_file);

        if (g_strcmp0 (name1, old_name->str) == 0)
        {
            new_name = l2->data;

            /* if the name didn't change, then there's a conflict */
            if (g_string_equal (old_name, new_name))
            {
                if (parent_uri == NULL)
                {
                    return FALSE;
                }

                g_autofree gchar *selection_parent_uri = NULL;

                selection_parent_uri = nautilus_file_get_parent_uri (selection_file);

                if (g_strcmp0 (parent_uri, selection_parent_uri) == 0)
                {
                    return FALSE;
                }
            }

            /* if this file exists and it changed it's name, then there's no
             * conflict */
            return TRUE;
        }
    }

    /* the case this function searched for doesn't exist, so the file
     * has a conlfict */
    return FALSE;
}

static gint
compare_files_by_name_ascending (gconstpointer a,
                                 gconstpointer b)
{
    NautilusFile *file1;
    NautilusFile *file2;

    file1 = NAUTILUS_FILE (a);
    file2 = NAUTILUS_FILE (b);

    return nautilus_file_compare_for_sort (file1, file2,
                                           NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
                                           FALSE, FALSE);
}

static gint
compare_files_by_name_descending (gconstpointer a,
                                  gconstpointer b)
{
    NautilusFile *file1;
    NautilusFile *file2;

    file1 = NAUTILUS_FILE (a);
    file2 = NAUTILUS_FILE (b);

    return nautilus_file_compare_for_sort (file1, file2,
                                           NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
                                           FALSE, TRUE);
}

static gint
compare_files_by_first_modified (gconstpointer a,
                                 gconstpointer b)
{
    NautilusFile *file1;
    NautilusFile *file2;

    file1 = NAUTILUS_FILE (a);
    file2 = NAUTILUS_FILE (b);

    return nautilus_file_compare_for_sort (file1, file2,
                                           NAUTILUS_FILE_SORT_BY_MTIME,
                                           FALSE, FALSE);
}

static gint
compare_files_by_last_modified (gconstpointer a,
                                gconstpointer b)
{
    NautilusFile *file1;
    NautilusFile *file2;

    file1 = NAUTILUS_FILE (a);
    file2 = NAUTILUS_FILE (b);

    return nautilus_file_compare_for_sort (file1, file2,
                                           NAUTILUS_FILE_SORT_BY_MTIME,
                                           FALSE, TRUE);
}

static gint
compare_files_by_first_created (gconstpointer a,
                                gconstpointer b)
{
    CreateDateElem *elem1;
    CreateDateElem *elem2;

    elem1 = (CreateDateElem *) a;
    elem2 = (CreateDateElem *) b;

    return elem1->position - elem2->position;
}

static gint
compare_files_by_last_created (gconstpointer a,
                               gconstpointer b)
{
    CreateDateElem *elem1;
    CreateDateElem *elem2;

    elem1 = (CreateDateElem *) a;
    elem2 = (CreateDateElem *) b;

    return elem2->position - elem1->position;
}

GList *
nautilus_batch_rename_dialog_sort (GList      *selection,
                                   SortMode    mode,
                                   GHashTable *creation_date_table)
{
    g_return_val_if_fail (selection != NULL, NULL);

    GList *l;

    if (mode == ORIGINAL_ASCENDING)
    {
        return g_list_sort (selection, compare_files_by_name_ascending);
    }

    if (mode == ORIGINAL_DESCENDING)
    {
        return g_list_sort (selection, compare_files_by_name_descending);
    }

    if (mode == FIRST_MODIFIED)
    {
        return g_list_sort (selection, compare_files_by_first_modified);
    }

    if (mode == LAST_MODIFIED)
    {
        return g_list_sort (selection, compare_files_by_last_modified);
    }

    if (mode == FIRST_CREATED || mode == LAST_CREATED)
    {
        g_autoptr (GPtrArray) create_date_list = g_ptr_array_new_with_free_func (g_free);
        guint i = 0;

        for (l = selection; l != NULL; l = l->next)
        {
            NautilusFile *file = NAUTILUS_FILE (l->data);
            g_autofree gchar *uri = nautilus_file_get_uri (file);
            CreateDateElem *elem = g_new (CreateDateElem, 1);

            elem->file = file;
            elem->position = GPOINTER_TO_INT (g_hash_table_lookup (creation_date_table, uri));

            g_ptr_array_add (create_date_list, elem);
        }

        if (mode == FIRST_CREATED)
        {
            g_ptr_array_sort_values (create_date_list, compare_files_by_first_created);
        }
        else
        {
            g_ptr_array_sort_values (create_date_list, compare_files_by_last_created);
        }

        for (l = selection, i = 0; i < create_date_list->len; l = l->next, i++)
        {
            CreateDateElem *elem = create_date_list->pdata[i];
            l->data = elem->file;
        }
    }

    return selection;
}

static void
cursor_next (QueryData           *query_data,
             TrackerSparqlCursor *cursor)
{
    tracker_sparql_cursor_next_async (cursor,
                                      query_data->cancellable,
                                      on_cursor_callback,
                                      query_data);
}

static void
on_cursor_callback (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
    TrackerSparqlCursor *cursor;
    gboolean success;
    QueryData *query_data;
    MetadataType metadata_type;
    g_autoptr (GError) error = NULL;
    FileMetadata *file_metadata;
    guint i;
    gconstpointer current_metadata;
    const gchar *file_uri;
    const gchar *file_name;
    g_autoptr (GDateTime) creation_datetime = NULL;
    const gchar *equipment;
    const gchar *season_number;
    const gchar *episode_number;
    const gchar *track_number;
    const gchar *artist_name;
    const gchar *title;
    const gchar *album_name;
    NautilusFile *file;

    file_metadata = NULL;

    cursor = TRACKER_SPARQL_CURSOR (object);
    query_data = user_data;

    success = tracker_sparql_cursor_next_finish (cursor, result, &error);
    if (!success)
    {
        if (error != NULL)
        {
            g_warning ("Error on batch rename tracker query cursor: %s", error->message);
        }

        g_clear_object (&cursor);

        /* The dialog is going away at the time of cancellation */
        if (error == NULL ||
            (error != NULL && error->code != G_IO_ERROR_CANCELLED))
        {
            nautilus_batch_rename_dialog_query_finished (query_data->dialog,
                                                         query_data->date_order_hash_table,
                                                         query_data->selection_metadata,
                                                         query_data->has_metadata);
        }

        g_free (query_data);

        return;
    }

    /* Search for the metadata object corresponding to the file */
    file_uri = tracker_sparql_cursor_get_string (cursor, URL_INDEX, NULL);
    file = nautilus_file_get_by_uri (file_uri);

    file_metadata = g_hash_table_lookup (query_data->selection_metadata, file);
    if (G_UNLIKELY (file_metadata == NULL))
    {
        g_warning ("Got a file that was not in the list of requested files");
        cursor_next (query_data, cursor);

        return;
    }

    if (tracker_sparql_cursor_get_value_type (cursor, CREATION_DATE_INDEX) == TRACKER_SPARQL_VALUE_TYPE_DATETIME)
    {
        creation_datetime = tracker_sparql_cursor_get_datetime (cursor, CREATION_DATE_INDEX);
    }
    equipment = tracker_sparql_cursor_get_string (cursor, CAMERA_MODEL_INDEX, NULL);
    season_number = tracker_sparql_cursor_get_string (cursor, SEASON_INDEX, NULL);
    episode_number = tracker_sparql_cursor_get_string (cursor, EPISODE_NUMBER_INDEX, NULL);
    track_number = tracker_sparql_cursor_get_string (cursor, TRACK_NUMBER_INDEX, NULL);
    artist_name = tracker_sparql_cursor_get_string (cursor, ARTIST_NAME_INDEX, NULL);
    title = tracker_sparql_cursor_get_string (cursor, TITLE_INDEX, NULL);
    album_name = tracker_sparql_cursor_get_string (cursor, ALBUM_NAME_INDEX, NULL);
    file_name = tracker_sparql_cursor_get_string (cursor, FILE_NAME_INDEX, NULL);

    /* Set metadata when available, and delete for the whole selection when not */
    for (i = 0; i < G_N_ELEMENTS (metadata_tags_constants); i++)
    {
        if (!query_data->has_metadata[i])
        {
            continue;
        }

        metadata_type = metadata_tags_constants[i].metadata_type;
        current_metadata = NULL;
        switch (metadata_type)
        {
            case ORIGINAL_FILE_NAME:
            {
                current_metadata = file_name;
            }
            break;

            case CREATION_DATE:
            {
                current_metadata = creation_datetime;
            }
            break;

            case EQUIPMENT:
            {
                current_metadata = equipment;
            }
            break;

            case SEASON_NUMBER:
            {
                current_metadata = season_number;
            }
            break;

            case EPISODE_NUMBER:
            {
                current_metadata = episode_number;
            }
            break;

            case ARTIST_NAME:
            {
                current_metadata = artist_name;
            }
            break;

            case ALBUM_NAME:
            {
                current_metadata = album_name;
            }
            break;

            case TITLE:
            {
                current_metadata = title;
            }
            break;

            case TRACK_NUMBER:
            {
                current_metadata = track_number;
            }
            break;

            default:
            {
                g_warn_if_reached ();
            }
            break;
        }

        if (!current_metadata)
        {
            query_data->has_metadata[i] = FALSE;

            if (metadata_type == CREATION_DATE &&
                query_data->date_order_hash_table)
            {
                g_hash_table_destroy (query_data->date_order_hash_table);
                query_data->date_order_hash_table = NULL;
            }
        }
        else
        {
            if (metadata_type == CREATION_DATE)
            {
                /* Add the sort order to the order hash table */
                g_hash_table_insert (query_data->date_order_hash_table,
                                     g_strdup (file_uri),
                                     GINT_TO_POINTER (g_hash_table_size (query_data->date_order_hash_table)));

                file_metadata->metadata[metadata_type] =
                    g_string_new_take (g_date_time_format (creation_datetime, "%F"));
            }
            else
            {
                if (file_metadata->metadata[metadata_type] != NULL)
                {
                    g_warning ("File %s has metadata type number %u being set but it already exists.",
                               file_uri, metadata_type);
                    g_string_free (file_metadata->metadata[metadata_type], TRUE);
                }

                file_metadata->metadata[metadata_type] = g_string_new (current_metadata);
                g_string_replace (file_metadata->metadata[metadata_type], "/", "_", 0);
            }
        }
    }

    /* Get next */
    cursor_next (query_data, cursor);
}

static void
batch_rename_dialog_query_callback (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
    TrackerSparqlConnection *connection;
    TrackerSparqlCursor *cursor;
    QueryData *query_data;
    g_autoptr (GError) error = NULL;

    connection = TRACKER_SPARQL_CONNECTION (object);
    query_data = user_data;

    cursor = tracker_sparql_connection_query_finish (connection,
                                                     result,
                                                     &error);

    if (error != NULL)
    {
        g_warning ("Error on batch rename query for metadata: %s", error->message);

        /* The dialog is being finalized at this point */
        if (error->code != G_IO_ERROR_CANCELLED)
        {
            nautilus_batch_rename_dialog_query_finished (query_data->dialog,
                                                         query_data->date_order_hash_table,
                                                         query_data->selection_metadata,
                                                         query_data->has_metadata);
        }

        g_free (query_data);
    }
    else
    {
        cursor_next (query_data, cursor);
    }
}

void
check_metadata_for_selection (NautilusBatchRenameDialog *dialog,
                              GList                     *selection,
                              GCancellable              *cancellable)
{
    TrackerSparqlConnection *connection;
    g_autoptr (GString) query = NULL;
    GList *l;
    GError *error;
    QueryData *query_data;
    FileMetadata *file_metadata;
    g_autoptr (GHashTable) selection_metadata = NULL;
    guint i;
    g_autofree gchar *parent_uri = NULL;
    gchar *file_name_escaped;

    connection = nautilus_localsearch_get_miner_fs_connection (&error);
    if (!connection)
    {
        if (error)
        {
            g_warning ("Error on batch rename tracker connection: %s", error->message);
            g_error_free (error);
        }

        return;
    }

    error = NULL;
    selection_metadata = g_hash_table_new_full (NULL, NULL,
                                                (GDestroyNotify) nautilus_file_unref, file_metadata_free);

    query = g_string_new ("SELECT DISTINCT "
                          "nfo:fileName(?file) "
                          "nie:contentCreated(?content) "
                          "nfo:model(nfo:equipment(?content)) "
                          "nmm:seasonNumber(?content) "
                          "nmm:episodeNumber(?content) "
                          "nmm:trackNumber(?content) "
                          "nmm:artistName(nmm:artist(?content)) "
                          "nie:title(?content) "
                          "nie:title(nmm:musicAlbum(?content)) "
                          "nie:url(?file) "
                          "WHERE { ?file a nfo:FileDataObject. ?file nie:url ?url. ?content nie:isStoredAs ?file. ");

    parent_uri = nautilus_file_get_parent_uri (NAUTILUS_FILE (selection->data));

    g_string_append_printf (query,
                            "FILTER(tracker:uri-is-parent(\"%s\", ?url)) ",
                            parent_uri);

    for (l = selection; l != NULL; l = l->next)
    {
        NautilusFile *file = NAUTILUS_FILE (l->data);
        const char *file_name = nautilus_file_get_name (file);
        file_name_escaped = tracker_sparql_escape_string (file_name);

        if (l == selection)
        {
            g_string_append_printf (query,
                                    "FILTER (nfo:fileName(?file) IN (\"%s\", ",
                                    file_name_escaped);
        }
        else if (l->next == NULL)
        {
            g_string_append_printf (query,
                                    "\"%s\")) ",
                                    file_name_escaped);
        }
        else
        {
            g_string_append_printf (query,
                                    "\"%s\", ",
                                    file_name_escaped);
        }

        file_metadata = g_new0 (FileMetadata, 1);
        file_metadata->file = file;

        g_hash_table_insert (selection_metadata,
                             nautilus_file_ref (file), file_metadata);

        g_free (file_name_escaped);
    }

    g_string_append (query, "} ORDER BY ASC(nie:contentCreated(?content))");

    query_data = g_new (QueryData, 1);
    query_data->date_order_hash_table = g_hash_table_new_full (g_str_hash,
                                                               g_str_equal,
                                                               (GDestroyNotify) g_free,
                                                               NULL);
    query_data->dialog = dialog;
    query_data->selection_metadata = g_steal_pointer (&selection_metadata);
    for (i = 0; i < G_N_ELEMENTS (metadata_tags_constants); i++)
    {
        query_data->has_metadata[i] = TRUE;
    }
    query_data->cancellable = cancellable;

    /* Make an asynchronous query to the store */
    tracker_sparql_connection_query_async (connection,
                                           query->str,
                                           cancellable,
                                           batch_rename_dialog_query_callback,
                                           query_data);
}

GList *
batch_rename_files_get_distinct_parents (GList *selection)
{
    GList *result;
    GList *l1;
    NautilusFile *file;
    NautilusDirectory *directory;

    result = NULL;
    for (l1 = selection; l1 != NULL; l1 = l1->next)
    {
        file = NAUTILUS_FILE (l1->data);
        directory = nautilus_file_get_directory (file);
        if (!g_list_find (result, directory))
        {
            result = g_list_prepend (result, nautilus_directory_ref (directory));
        }
    }

    return result;
}
