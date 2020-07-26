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
#include "nautilus-file.h"
#include "nautilus-filename-utilities.h"
#include "nautilus-tracker-utilities.h"

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

    GList *selection_metadata;

    gboolean has_metadata[G_N_ELEMENTS (metadata_tags_constants)];

    GCancellable *cancellable;
} QueryData;

enum
{
    FILE_NAME_INDEX,
    CREATION_DATE_INDEX,
    YEAR_INDEX,
    MONTH_INDEX,
    DAY_INDEX,
    HOURS_INDEX,
    MINUTES_INDEX,
    SECONDS_INDEX,
    CAMERA_MODEL_INDEX,
    SEASON_INDEX,
    EPISODE_NUMBER_INDEX,
    TRACK_NUMBER_INDEX,
    ARTIST_NAME_INDEX,
    TITLE_INDEX,
    ALBUM_NAME_INDEX,
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

gchar *
batch_rename_get_tag_text_representation (TagConstants tag_constants)
{
    return g_strdup_printf ("[%s]", gettext (tag_constants.label));
}

static GString *
batch_rename_replace (gchar *string,
                      gchar *substring,
                      gchar *replacement)
{
    GString *new_string;
    gchar **splitted_string;
    gint i, n_splits;

    new_string = g_string_new ("");

    if (substring == NULL || replacement == NULL)
    {
        g_string_append (new_string, string);

        return new_string;
    }

    if (g_utf8_strlen (substring, -1) == 0)
    {
        g_string_append (new_string, string);

        return new_string;
    }

    splitted_string = g_strsplit (string, substring, -1);
    if (splitted_string == NULL)
    {
        g_string_append (new_string, string);

        return new_string;
    }

    n_splits = g_strv_length (splitted_string);

    for (i = 0; i < n_splits; i++)
    {
        g_string_append (new_string, splitted_string[i]);

        if (i != n_splits - 1)
        {
            g_string_append (new_string, replacement);
        }
    }

    g_strfreev (splitted_string);

    return new_string;
}

/* This function changes the background color of the replaced part of the name */
GString *
batch_rename_replace_label_text (const char  *label,
                                 const gchar *substring)
{
    GString *new_label;
    gchar **splitted_string;
    gchar *token;
    gint i, n_splits;

    new_label = g_string_new ("");

    if (substring == NULL || g_strcmp0 (substring, "") == 0)
    {
        token = g_markup_escape_text (label, -1);
        new_label = g_string_append (new_label, token);
        g_free (token);

        return new_label;
    }

    splitted_string = g_strsplit (label, substring, -1);
    if (splitted_string == NULL)
    {
        token = g_markup_escape_text (label, -1);
        new_label = g_string_append (new_label, token);
        g_free (token);

        return new_label;
    }

    n_splits = g_strv_length (splitted_string);

    for (i = 0; i < n_splits; i++)
    {
        token = g_markup_escape_text (splitted_string[i], -1);
        new_label = g_string_append (new_label, token);

        g_free (token);

        if (i != n_splits - 1)
        {
            token = g_markup_escape_text (substring, -1);
            g_string_append_printf (new_label,
                                    "<span background=\'#f57900\' color='white'>%s</span>",
                                    token);

            g_free (token);
        }
    }

    g_strfreev (splitted_string);

    return new_label;
}

static gchar *
get_metadata (GList        *selection_metadata,
              const char   *file_name,
              MetadataType  metadata_type)
{
    GList *l;
    FileMetadata *file_metadata;
    gchar *metadata = NULL;

    for (l = selection_metadata; l != NULL; l = l->next)
    {
        file_metadata = l->data;
        if (g_strcmp0 (file_name, file_metadata->file_name->str) == 0)
        {
            if (file_metadata->metadata[metadata_type] &&
                file_metadata->metadata[metadata_type]->len > 0)
            {
                metadata = file_metadata->metadata[metadata_type]->str;
            }

            break;
        }
    }

    return metadata;
}

static GString *
batch_rename_format (NautilusFile *file,
                     GList        *text_chunks,
                     GList        *selection_metadata,
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
            g_autofree gchar *tag_text_representation = NULL;

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
            g_autofree gchar *tag_text_representation = NULL;

            tag_text_representation = batch_rename_get_tag_text_representation (metadata_tags_constants[i]);
            if (g_strcmp0 (tag_string->str, tag_text_representation) == 0)
            {
                metadata_type = metadata_tags_constants[i].metadata_type;
                metadata = get_metadata (selection_metadata, file_name, metadata_type);

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
                                        GList                         *selection_metadata,
                                        gchar                         *entry_text,
                                        gchar                         *replace_text)
{
    GList *l;
    GList *result;
    GString *file_name;
    GString *new_name;
    gint count;

    result = NULL;
    count = 1;

    for (l = selection; l != NULL; l = l->next)
    {
        NautilusFile *file = NAUTILUS_FILE (l->data);
        const char *name = nautilus_file_get_name (file);

        file_name = g_string_new (name);

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
            new_name = batch_rename_replace (file_name->str,
                                             entry_text,
                                             replace_text);
            result = g_list_prepend (result, new_name);
        }

        g_string_free (file_name, TRUE);
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
        g_autofree gchar *selection_parent_uri = NULL;

        selection_parent_uri = nautilus_file_get_parent_uri (selection_file);

        if (g_strcmp0 (name1, old_name->str) == 0)
        {
            new_name = l2->data;

            /* if the name didn't change, then there's a conflict */
            if (g_string_equal (old_name, new_name) &&
                (parent_uri == NULL || g_strcmp0 (parent_uri, selection_parent_uri) == 0))
            {
                return FALSE;
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
    GList *l, *l2;
    GList *create_date_list;
    GList *create_date_list_sorted;

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
        create_date_list = NULL;

        for (l = selection; l != NULL; l = l->next)
        {
            NautilusFile *file = NAUTILUS_FILE (l->data);
            const char *name = nautilus_file_get_name (file);
            CreateDateElem *elem = g_new (CreateDateElem, 1);

            elem->file = file;
            elem->position = GPOINTER_TO_INT (g_hash_table_lookup (creation_date_table, name));

            create_date_list = g_list_prepend (create_date_list, elem);
        }

        if (mode == FIRST_CREATED)
        {
            create_date_list_sorted = g_list_sort (create_date_list,
                                                   compare_files_by_first_created);
        }
        else
        {
            create_date_list_sorted = g_list_sort (create_date_list,
                                                   compare_files_by_last_created);
        }

        for (l = selection, l2 = create_date_list_sorted; l2 != NULL; l = l->next, l2 = l2->next)
        {
            CreateDateElem *elem = l2->data;
            l->data = elem->file;
        }

        g_list_free_full (create_date_list, g_free);
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
remove_metadata (QueryData    *query_data,
                 MetadataType  metadata_type)
{
    GList *l;
    FileMetadata *metadata_to_delete;

    for (l = query_data->selection_metadata; l != NULL; l = l->next)
    {
        metadata_to_delete = l->data;
        if (metadata_to_delete->metadata[metadata_type])
        {
            g_string_free (metadata_to_delete->metadata[metadata_type], TRUE);
            metadata_to_delete->metadata[metadata_type] = NULL;
        }
    }

    query_data->has_metadata[metadata_type] = FALSE;
}

static GString *
format_date_time (GDateTime *date_time)
{
    g_autofree gchar *date = NULL;
    GString *formated_date;

    date = g_date_time_format (date_time, "%x");
    if (strstr (date, "/") != NULL)
    {
        formated_date = batch_rename_replace (date, "/", "-");
    }
    else
    {
        formated_date = g_string_new (date);
    }

    return formated_date;
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
    GList *l;
    FileMetadata *file_metadata;
    GDateTime *date_time;
    guint i;
    const gchar *current_metadata;
    const gchar *file_name;
    const gchar *creation_date;
    const gchar *year;
    const gchar *month;
    const gchar *day;
    const gchar *hours;
    const gchar *minutes;
    const gchar *seconds;
    const gchar *equipment;
    const gchar *season_number;
    const gchar *episode_number;
    const gchar *track_number;
    const gchar *artist_name;
    const gchar *title;
    const gchar *album_name;

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
                                                         query_data->selection_metadata);
        }

        g_free (query_data);

        return;
    }

    creation_date = tracker_sparql_cursor_get_string (cursor, CREATION_DATE_INDEX, NULL);

    year = tracker_sparql_cursor_get_string (cursor, YEAR_INDEX, NULL);
    month = tracker_sparql_cursor_get_string (cursor, MONTH_INDEX, NULL);
    day = tracker_sparql_cursor_get_string (cursor, DAY_INDEX, NULL);
    hours = tracker_sparql_cursor_get_string (cursor, HOURS_INDEX, NULL);
    minutes = tracker_sparql_cursor_get_string (cursor, MINUTES_INDEX, NULL);
    seconds = tracker_sparql_cursor_get_string (cursor, SECONDS_INDEX, NULL);
    equipment = tracker_sparql_cursor_get_string (cursor, CAMERA_MODEL_INDEX, NULL);
    season_number = tracker_sparql_cursor_get_string (cursor, SEASON_INDEX, NULL);
    episode_number = tracker_sparql_cursor_get_string (cursor, EPISODE_NUMBER_INDEX, NULL);
    track_number = tracker_sparql_cursor_get_string (cursor, TRACK_NUMBER_INDEX, NULL);
    artist_name = tracker_sparql_cursor_get_string (cursor, ARTIST_NAME_INDEX, NULL);
    title = tracker_sparql_cursor_get_string (cursor, TITLE_INDEX, NULL);
    album_name = tracker_sparql_cursor_get_string (cursor, ALBUM_NAME_INDEX, NULL);

    /* Search for the metadata object corresponding to the file name */
    file_name = tracker_sparql_cursor_get_string (cursor, FILE_NAME_INDEX, NULL);
    for (l = query_data->selection_metadata; l != NULL; l = l->next)
    {
        file_metadata = l->data;

        if (g_strcmp0 (file_name, file_metadata->file_name->str) == 0)
        {
            break;
        }
    }

    /* Set metadata when available, and delete for the whole selection when not */
    for (i = 0; i < G_N_ELEMENTS (metadata_tags_constants); i++)
    {
        if (query_data->has_metadata[i])
        {
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
                    current_metadata = creation_date;
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

            /* TODO: Figure out how to inform the user of why the metadata is
             * unavailable when one or more contains the unallowed character "/"
             */
            if (!current_metadata || g_strrstr (current_metadata, "/"))
            {
                remove_metadata (query_data,
                                 metadata_type);

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
                                         g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL)),
                                         GINT_TO_POINTER (g_hash_table_size (query_data->date_order_hash_table)));

                    date_time = g_date_time_new_local (atoi (year),
                                                       atoi (month),
                                                       atoi (day),
                                                       atoi (hours),
                                                       atoi (minutes),
                                                       atoi (seconds));

                    file_metadata->metadata[metadata_type] = format_date_time (date_time);
                }
                else
                {
                    file_metadata->metadata[metadata_type] = g_string_new (current_metadata);
                }
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
                                                         query_data->selection_metadata);
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
    GList *selection_metadata;
    guint i;
    g_autofree gchar *parent_uri = NULL;
    gchar *file_name_escaped;

    connection = nautilus_tracker_get_miner_fs_connection (&error);
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
    selection_metadata = NULL;

    query = g_string_new ("SELECT "
                          "nfo:fileName(?file) "
                          "nie:contentCreated(?content) "
                          "year(nie:contentCreated(?content)) "
                          "month(nie:contentCreated(?content)) "
                          "day(nie:contentCreated(?content)) "
                          "hours(nie:contentCreated(?content)) "
                          "minutes(nie:contentCreated(?content)) "
                          "seconds(nie:contentCreated(?content)) "
                          "nfo:model(nfo:equipment(?content)) "
                          "nmm:seasonNumber(?content) "
                          "nmm:episodeNumber(?content) "
                          "nmm:trackNumber(?content) "
                          "nmm:artistName(nmm:performer(?content)) "
                          "nie:title(?content) "
                          "nie:title(nmm:musicAlbum(?content)) "
                          "WHERE { ?file a nfo:FileDataObject. ?file nie:url ?url. ?content nie:isStoredAs ?file. ");

    parent_uri = nautilus_file_get_parent_uri (NAUTILUS_FILE (selection->data));

    g_string_append_printf (query,
                            "FILTER(tracker:uri-is-parent(\"%s\", ?url)) ",
                            parent_uri);

    for (l = selection; l != NULL; l = l->next)
    {
        const char *file_name = nautilus_file_get_name (NAUTILUS_FILE (l->data));
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
        file_metadata->file_name = g_string_new (file_name);
        file_metadata->metadata[ORIGINAL_FILE_NAME] = g_string_new (file_name);

        selection_metadata = g_list_prepend (selection_metadata, file_metadata);

        g_free (file_name_escaped);
    }

    selection_metadata = g_list_reverse (selection_metadata);

    g_string_append (query, "} ORDER BY ASC(nie:contentCreated(?content))");

    query_data = g_new (QueryData, 1);
    query_data->date_order_hash_table = g_hash_table_new_full (g_str_hash,
                                                               g_str_equal,
                                                               (GDestroyNotify) g_free,
                                                               NULL);
    query_data->dialog = dialog;
    query_data->selection_metadata = selection_metadata;
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
    NautilusFile *parent;

    result = NULL;
    for (l1 = selection; l1 != NULL; l1 = l1->next)
    {
        file = NAUTILUS_FILE (l1->data);
        parent = nautilus_file_get_parent (file);
        directory = nautilus_directory_get_for_file (parent);
        if (!g_list_find (result, directory))
        {
            result = g_list_prepend (result, directory);
        }

        nautilus_file_unref (parent);
    }

    return result;
}
