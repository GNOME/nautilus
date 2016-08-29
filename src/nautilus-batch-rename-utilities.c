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

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdarg.h>
#include <eel/eel-vfs-extensions.h>

typedef struct
{
    NautilusFile *file;
    gint position;
} CreateDateElem;

typedef struct
{
    NautilusBatchRenameDialog *dialog;
    GHashTable *hash_table;

    GList *selection_metadata;

    gboolean have_creation_date;
    gboolean have_equipment;
    gboolean have_season;
    gboolean have_episode_number;
    gboolean have_track_number;
    gboolean have_artist_name;
    gboolean have_title;
    gboolean have_album_name;
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
batch_rename_replace_label_text (gchar       *label,
                                 const gchar *substring)
{
    GString *new_label;
    gchar **splitted_string;
    gchar *token;
    gint i, n_splits;

    new_label = g_string_new ("");

    if (substring == NULL || g_strcmp0 (substring, "") == 0)
    {
        token = g_markup_escape_text (label, g_utf8_strlen (label, -1));
        new_label = g_string_append (new_label, token);
        g_free (token);

        return new_label;
    }

    splitted_string = g_strsplit (label, substring, -1);
    if (splitted_string == NULL)
    {
        token = g_markup_escape_text (label, g_utf8_strlen (label, -1));
        new_label = g_string_append (new_label, token);
        g_free (token);

        return new_label;
    }

    n_splits = g_strv_length (splitted_string);

    for (i = 0; i < n_splits; i++)
    {
        token = g_markup_escape_text (splitted_string[i], strlen (splitted_string[i]));
        new_label = g_string_append (new_label, token);

        g_free (token);

        if (i != n_splits - 1)
        {
            token = g_markup_escape_text (substring, g_utf8_strlen (substring, -1));
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
get_metadata (GList *selection_metadata,
              gchar *file_name,
              gchar *metadata)
{
    GList *l;
    FileMetadata *file_metadata;

    for (l = selection_metadata; l != NULL; l = l->next)
    {
        file_metadata = l->data;
        if (g_strcmp0 (file_name, file_metadata->file_name->str) == 0)
        {
            if (g_strcmp0 (metadata, "creation_date") == 0 &&
                file_metadata->creation_date != NULL &&
                file_metadata->creation_date->len != 0)
            {
                return file_metadata->creation_date->str;
            }

            if (g_strcmp0 (metadata, "equipment") == 0 &&
                file_metadata->equipment != NULL &&
                file_metadata->equipment->len != 0)
            {
                return file_metadata->equipment->str;
            }

            if (g_strcmp0 (metadata, "season") == 0 &&
                file_metadata->season != NULL &&
                file_metadata->season->len != 0)
            {
                return file_metadata->season->str;
            }

            if (g_strcmp0 (metadata, "episode_number") == 0 &&
                file_metadata->episode_number != NULL &&
                file_metadata->episode_number->len != 0)
            {
                return file_metadata->episode_number->str;
            }

            if (g_strcmp0 (metadata, "track_number") == 0 &&
                file_metadata->track_number != NULL &&
                file_metadata->track_number->len != 0)
            {
                return file_metadata->track_number->str;
            }

            if (g_strcmp0 (metadata, "artist_name") == 0 &&
                file_metadata->artist_name != NULL &&
                file_metadata->artist_name->len != 0)
            {
                return file_metadata->artist_name->str;
            }

            if (g_strcmp0 (metadata, "title") == 0 &&
                file_metadata->title != NULL &&
                file_metadata->title->len != 0)
            {
                return file_metadata->title->str;
            }

            if (g_strcmp0 (metadata, "album_name") == 0 &&
                file_metadata->album_name != NULL &&
                file_metadata->album_name->len != 0)
            {
                return file_metadata->album_name->str;
            }
        }
    }

    return NULL;
}

static GString *
batch_rename_format (NautilusFile *file,
                     GList        *text_chunks,
                     GList        *selection_metadata,
                     gint          count)
{
    GList *l;
    GString *tag;
    GString *new_name;
    gboolean added_tag;
    g_autofree gchar *file_name;
    g_autofree gchar *extension;
    gchar *metadata;
    gchar *base_name;

    file_name = nautilus_file_get_display_name (file);
    extension = nautilus_file_get_extension (file);

    new_name = g_string_new ("");

    for (l = text_chunks; l != NULL; l = l->next)
    {
        tag = l->data;
        added_tag = FALSE;

        if (!added_tag && g_strcmp0 (tag->str, ORIGINAL_FILE_NAME) == 0)
        {
            base_name = eel_filename_strip_extension (file_name);

            new_name = g_string_append (new_name, base_name);

            added_tag = TRUE;
            g_free (base_name);
        }

        if (!added_tag && g_strcmp0 (tag->str, NUMBERING) == 0)
        {
            g_string_append_printf (new_name, "%d", count);
            added_tag = TRUE;
        }

        if (!added_tag && g_strcmp0 (tag->str, NUMBERING0) == 0)
        {
            g_string_append_printf (new_name, "%02d", count);

            added_tag = TRUE;
        }

        if (!added_tag && g_strcmp0 (tag->str, NUMBERING00) == 0)
        {
            g_string_append_printf (new_name, "%03d", count);

            added_tag = TRUE;
        }

        if (!added_tag && g_strcmp0 (tag->str, CAMERA_MODEL) == 0)
        {
            metadata = get_metadata (selection_metadata, file_name, "equipment");

            if (metadata != NULL)
            {
                new_name = g_string_append (new_name, metadata);
                added_tag = TRUE;
            }
        }

        if (!added_tag && g_strcmp0 (tag->str, CREATION_DATE) == 0)
        {
            metadata = get_metadata (selection_metadata, file_name, "creation_date");

            if (metadata != NULL)
            {
                new_name = g_string_append (new_name, metadata);
                added_tag = TRUE;
            }
        }

        if (!added_tag && g_strcmp0 (tag->str, SEASON_NUMBER) == 0)
        {
            metadata = get_metadata (selection_metadata, file_name, "season");

            if (metadata != NULL)
            {
                new_name = g_string_append (new_name, metadata);
                added_tag = TRUE;
            }
        }

        if (!added_tag && g_strcmp0 (tag->str, EPISODE_NUMBER) == 0)
        {
            metadata = get_metadata (selection_metadata, file_name, "episode_number");

            if (metadata != NULL)
            {
                new_name = g_string_append (new_name, metadata);
                added_tag = TRUE;
            }
        }

        if (!added_tag && g_strcmp0 (tag->str, TRACK_NUMBER) == 0)
        {
            metadata = get_metadata (selection_metadata, file_name, "track_number");

            if (metadata != NULL)
            {
                new_name = g_string_append (new_name, metadata);
                added_tag = TRUE;
            }
        }

        if (!added_tag && g_strcmp0 (tag->str, ARTIST_NAME) == 0)
        {
            metadata = get_metadata (selection_metadata, file_name, "artist_name");

            if (metadata != NULL)
            {
                new_name = g_string_append (new_name, metadata);
                added_tag = TRUE;
            }
        }

        if (!added_tag && g_strcmp0 (tag->str, TITLE) == 0)
        {
            metadata = get_metadata (selection_metadata, file_name, "title");

            if (metadata != NULL)
            {
                new_name = g_string_append (new_name, metadata);
                added_tag = TRUE;
            }
        }

        if (!added_tag && g_strcmp0 (tag->str, ALBUM_NAME) == 0)
        {
            metadata = get_metadata (selection_metadata, file_name, "album_name");

            if (metadata != NULL)
            {
                new_name = g_string_append (new_name, metadata);
                added_tag = TRUE;
            }
        }

        if (!added_tag)
        {
            new_name = g_string_append (new_name, tag->str);
        }
    }

    if (g_strcmp0 (new_name->str, "") == 0)
    {
        new_name = g_string_append (new_name, file_name);
    }
    else
    {
        if (extension != NULL)
        {
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
    NautilusFile *file;
    gchar *name;
    gint count;

    result = NULL;
    count = 1;
    file_name = g_string_new ("");

    for (l = selection; l != NULL; l = l->next)
    {
        file = NAUTILUS_FILE (l->data);

        file_name = g_string_new ("");
        name = nautilus_file_get_name (file);
        g_string_append (file_name, name);

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
        g_free (name);
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
    NautilusFile *selection_file;
    gchar *name1;
    GString *new_name;
    gchar *selection_parent_uri;

    for (l1 = selection, l2 = new_names; l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next)
    {
        selection_file = NAUTILUS_FILE (l1->data);
        name1 = nautilus_file_get_name (selection_file);

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

        g_free (selection_parent_uri);
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
nautilus_batch_rename_dialog_sort (GList       *selection,
                                   SortingMode  mode,
                                   GHashTable  *creation_date_table)
{
    GList *l, *l2;
    NautilusFile *file;
    GList *create_date_list;
    GList *create_date_list_sorted;
    gchar *name;

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
            CreateDateElem *elem;
            elem = g_new (CreateDateElem, 1);

            file = NAUTILUS_FILE (l->data);

            name = nautilus_file_get_name (file);
            elem->file = file;
            elem->position = GPOINTER_TO_INT (g_hash_table_lookup (creation_date_table, name));
            g_free (name);

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
                                      NULL,
                                      on_cursor_callback,
                                      query_data);
}

static void
on_cursor_callback (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
    GHashTable *hash_table;
    TrackerSparqlCursor *cursor;
    gboolean success;
    QueryData *query_data;
    GError *error;
    GList *l;
    FileMetadata *metadata;
    FileMetadata *metadata_clear;
    GDateTime *datetime;
    gchar *date;
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

    error = NULL;
    metadata = NULL;

    cursor = TRACKER_SPARQL_CURSOR (object);
    query_data = user_data;
    hash_table = query_data->hash_table;

    success = tracker_sparql_cursor_next_finish (cursor, result, &error);
    if (!success)
    {
        g_clear_error (&error);
        g_clear_object (&cursor);

        nautilus_batch_rename_dialog_query_finished (query_data->dialog,
                                                     query_data->hash_table,
                                                     query_data->selection_metadata);

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

    /* creation date used for sorting criteria */
    if (creation_date == NULL)
    {
        if (hash_table != NULL)
        {
            g_hash_table_destroy (hash_table);
        }

        query_data->hash_table = NULL;
        query_data->have_creation_date = FALSE;
    }
    else
    {
        if (query_data->have_creation_date)
        {
            g_hash_table_insert (hash_table,
                                 g_strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL)),
                                 GINT_TO_POINTER (g_hash_table_size (hash_table)));
        }
    }
    file_name = tracker_sparql_cursor_get_string (cursor, FILE_NAME_INDEX, NULL);
    for (l = query_data->selection_metadata; l != NULL; l = l->next)
    {
        metadata = l->data;

        if (g_strcmp0 (file_name, metadata->file_name->str) == 0)
        {
            break;
        }
    }

    /* Metadata to be used in file name
     * creation date */
    if (query_data->have_creation_date)
    {
        if (!creation_date)
        {
            query_data->have_creation_date = FALSE;

            for (l = query_data->selection_metadata; l != NULL; l = l->next)
            {
                metadata_clear = l->data;

                if (metadata_clear->creation_date != NULL)
                {
                    g_string_free (metadata_clear->creation_date, TRUE);
                    metadata_clear->creation_date = NULL;
                }
            }
        }
        else
        {
            datetime = g_date_time_new_local (atoi (year),
                                              atoi (month),
                                              atoi (day),
                                              atoi (hours),
                                              atoi (minutes),
                                              atoi (seconds));

            date = g_date_time_format (datetime, "%x");

            if (strstr (date, "/") != NULL)
            {
                metadata->creation_date = batch_rename_replace (date, "/", "-");
            }
            else
            {
                metadata->creation_date = g_string_new (date);
            }

            g_free (date);
        }
    }

    /* equipment */
    if (query_data->have_equipment)
    {
        if (equipment == NULL)
        {
            query_data->have_equipment = FALSE;

            for (l = query_data->selection_metadata; l != NULL; l = l->next)
            {
                metadata_clear = l->data;

                if (metadata_clear->equipment != NULL)
                {
                    g_string_free (metadata_clear->equipment, TRUE);
                    metadata_clear->equipment = NULL;
                }
            }
        }
        else
        {
            metadata->equipment = g_string_new (equipment);
        }
    }

    /* season number */
    if (query_data->have_season)
    {
        if (season_number == NULL)
        {
            query_data->have_season = FALSE;

            for (l = query_data->selection_metadata; l != NULL; l = l->next)
            {
                metadata_clear = l->data;

                if (metadata_clear->season != NULL)
                {
                    g_string_free (metadata_clear->season, TRUE);
                    metadata_clear->season = NULL;
                }
            }
        }
        else
        {
            metadata->season = g_string_new (season_number);
        }
    }

    /* episode number */
    if (query_data->have_episode_number)
    {
        if (episode_number == NULL)
        {
            query_data->have_episode_number = FALSE;

            for (l = query_data->selection_metadata; l != NULL; l = l->next)
            {
                metadata_clear = l->data;

                if (metadata_clear->episode_number != NULL)
                {
                    g_string_free (metadata_clear->episode_number, TRUE);
                    metadata_clear->episode_number = NULL;
                }
            }
        }
        else
        {
            metadata->episode_number = g_string_new (episode_number);
        }
    }

    /* track number */
    if (query_data->have_track_number)
    {
        if (track_number == NULL)
        {
            query_data->have_track_number = FALSE;
            for (l = query_data->selection_metadata; l != NULL; l = l->next)
            {
                metadata_clear = l->data;

                if (metadata_clear->track_number != NULL)
                {
                    g_string_free (metadata_clear->track_number, TRUE);
                    metadata_clear->track_number = NULL;
                }
            }
        }
        else
        {
            metadata->track_number = g_string_new (track_number);
        }
    }

    /* artist name */
    if (query_data->have_artist_name)
    {
        if (artist_name == NULL)
        {
            query_data->have_artist_name = FALSE;

            for (l = query_data->selection_metadata; l != NULL; l = l->next)
            {
                metadata_clear = l->data;

                if (metadata_clear->artist_name != NULL)
                {
                    g_string_free (metadata_clear->artist_name, TRUE);
                    metadata_clear->artist_name = NULL;
                }
            }
        }
        else
        {
            metadata->artist_name = g_string_new (artist_name);
        }
    }

    /* title */
    if (query_data->have_title)
    {
        if (title == NULL)
        {
            query_data->have_title = FALSE;

            for (l = query_data->selection_metadata; l != NULL; l = l->next)
            {
                metadata_clear = l->data;

                if (metadata_clear->title != NULL)
                {
                    g_string_free (metadata_clear->title, TRUE);
                    metadata_clear->title = NULL;
                }
            }
        }
        else
        {
            metadata->title = g_string_new (title);
        }
    }

    /* album name */
    if (query_data->have_album_name)
    {
        if (album_name == NULL)
        {
            query_data->have_album_name = FALSE;

            for (l = query_data->selection_metadata; l != NULL; l = l->next)
            {
                metadata_clear = l->data;

                if (metadata_clear->album_name != NULL)
                {
                    g_string_free (metadata_clear->album_name, TRUE);
                    metadata_clear->album_name = NULL;
                }
            }
        }
        else
        {
            metadata->album_name = g_string_new (album_name);
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
    GError *error;

    error = NULL;

    connection = TRACKER_SPARQL_CONNECTION (object);
    query_data = user_data;

    cursor = tracker_sparql_connection_query_finish (connection,
                                                     result,
                                                     &error);

    if (error != NULL)
    {
        g_warning ("Error on batch rename query for metadata: %s", error->message);
        g_error_free (error);

        nautilus_batch_rename_dialog_query_finished (query_data->dialog,
                                                     query_data->hash_table,
                                                     query_data->selection_metadata);
    }
    else
    {
        cursor_next (query_data, cursor);
    }
}

void
check_metadata_for_selection (NautilusBatchRenameDialog *dialog,
                              GList                     *selection)
{
    TrackerSparqlConnection *connection;
    GString *query;
    GHashTable *hash_table;
    GList *l;
    NautilusFile *file;
    GError *error;
    QueryData *query_data;
    gchar *file_name;
    FileMetadata *metadata;
    GList *selection_metadata;

    error = NULL;
    selection_metadata = NULL;

    query = g_string_new ("SELECT "
                          "nfo:fileName(?file) "
                          "nie:contentCreated(?file) "
                          "year(nie:contentCreated(?file)) "
                          "month(nie:contentCreated(?file)) "
                          "day(nie:contentCreated(?file)) "
                          "hours(nie:contentCreated(?file)) "
                          "minutes(nie:contentCreated(?file)) "
                          "seconds(nie:contentCreated(?file)) "
                          "nfo:model(nfo:equipment(?file)) "
                          "nmm:season(?file) "
                          "nmm:episodeNumber(?file) "
                          "nmm:trackNumber(?file) "
                          "nmm:artistName(nmm:performer(?file)) "
                          "nie:title(?file) "
                          "nmm:albumTitle(nmm:musicAlbum(?file)) "
                          "WHERE { ?file a nfo:FileDataObject. ");

    g_string_append_printf (query,
                            "FILTER(tracker:uri-is-parent('%s', nie:url(?file))) ",
                            nautilus_file_get_parent_uri (NAUTILUS_FILE (selection->data)));

    for (l = selection; l != NULL; l = l->next)
    {
        file = NAUTILUS_FILE (l->data);
        file_name = nautilus_file_get_name (file);

        if (l == selection)
        {
            g_string_append_printf (query,
                                    "FILTER (nfo:fileName(?file) = '%s' ",
                                    file_name);
        }
        else
        {
            g_string_append_printf (query,
                                    "|| nfo:fileName(?file) = '%s' ",
                                    file_name);
        }

        metadata = g_new (FileMetadata, 1);
        metadata->file_name = g_string_new (file_name);
        metadata->creation_date = NULL;
        metadata->equipment = NULL;
        metadata->season = NULL;
        metadata->episode_number = NULL;
        metadata->track_number = NULL;
        metadata->artist_name = NULL;
        metadata->title = NULL;
        metadata->album_name = NULL;

        selection_metadata = g_list_append (selection_metadata, metadata);

        g_free (file_name);
    }

    g_string_append (query, ")} ORDER BY ASC(nie:contentCreated(?file))");

    connection = tracker_sparql_connection_get (NULL, &error);
    if (!connection)
    {
        g_error_free (error);

        return;
    }

    hash_table = g_hash_table_new_full (g_str_hash,
                                        g_str_equal,
                                        (GDestroyNotify) g_free,
                                        NULL);

    query_data = g_new (QueryData, 1);
    query_data->hash_table = hash_table;
    query_data->dialog = dialog;
    query_data->selection_metadata = selection_metadata;

    query_data->have_season = TRUE;
    query_data->have_creation_date = TRUE;
    query_data->have_artist_name = TRUE;
    query_data->have_track_number = TRUE;
    query_data->have_equipment = TRUE;
    query_data->have_episode_number = TRUE;
    query_data->have_title = TRUE;
    query_data->have_album_name = TRUE;

    /* Make an asynchronous query to the store */
    tracker_sparql_connection_query_async (connection,
                                           query->str,
                                           NULL,
                                           batch_rename_dialog_query_callback,
                                           query_data);

    g_object_unref (connection);
    g_string_free (query, TRUE);
}

GList *
batch_rename_files_get_distinct_parents (GList *selection)
{
    GList *result;
    GList *l1;
    GList *l2;
    NautilusFile *file;
    gboolean exists;
    gchar *parent_uri;

    result = NULL;

    for (l1 = selection; l1 != NULL; l1 = l1->next)
    {
        exists = FALSE;

        file = NAUTILUS_FILE (l1->data);
        parent_uri = nautilus_file_get_parent_uri (file);

        for (l2 = result; l2 != NULL; l2 = l2->next)
        {
            if (g_strcmp0 (parent_uri, l2->data) == 0)
            {
                exists = TRUE;
                break;
            }
        }

        if (!exists)
        {
            result = g_list_prepend (result, parent_uri);
        }
        else
        {
            g_free (parent_uri);
        }
    }

    return result;
}
