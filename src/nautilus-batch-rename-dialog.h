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

#ifndef NAUTILUS_BATCH_RENAME_DIALOG_H
#define NAUTILUS_BATCH_RENAME_DIALOG_H

#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include "nautilus-files-view.h"

G_BEGIN_DECLS

#define ORIGINAL_FILE_NAME "[Original file name]"
#define NUMBERING "[1, 2, 3]"
#define NUMBERING0 "[01, 02, 03]"
#define NUMBERING00 "[001, 002, 003]"
#define CAMERA_MODEL "[Camera model]"
#define CREATION_DATE "[Creation date]"
#define SEASON_NUMBER "[Season number]"
#define EPISODE_NUMBER "[Episode number]"
#define TRACK_NUMBER "[Track number]"
#define ARTIST_NAME "[Artist name]"
#define TITLE "[Title]"
#define ALBUM_NAME "[Album name]"

typedef enum {
        NAUTILUS_BATCH_RENAME_DIALOG_APPEND = 0,
        NAUTILUS_BATCH_RENAME_DIALOG_PREPEND = 1,
        NAUTILUS_BATCH_RENAME_DIALOG_REPLACE = 2,
        NAUTILUS_BATCH_RENAME_DIALOG_FORMAT = 3,
} NautilusBatchRenameDialogMode;

typedef enum {
        ORIGINAL_ASCENDING = 0,
        ORIGINAL_DESCENDING = 1,
        FIRST_MODIFIED = 2,
        LAST_MODIFIED = 3,
        FIRST_CREATED = 4,
        LAST_CREATED = 5,
} SortMode;

typedef struct
{
        gchar *name;
        gint index;
} ConflictData;

typedef struct {
        GString *file_name;

        /* Photo */
        GString *creation_date;
        GString *equipment;

        /* Video */
        GString *season;
        GString *episode_number;

        /* Music */
        GString *track_number;
        GString *artist_name;
        GString *title;
        GString *album_name;
} FileMetadata;

#define NAUTILUS_TYPE_BATCH_RENAME_DIALOG (nautilus_batch_rename_dialog_get_type())

G_DECLARE_FINAL_TYPE (NautilusBatchRenameDialog, nautilus_batch_rename_dialog, NAUTILUS, BATCH_RENAME_DIALOG, GtkDialog);

GtkWidget*      nautilus_batch_rename_dialog_new                      (GList                     *selection,
                                                                       NautilusDirectory         *directory,
                                                                       NautilusWindow            *window);

void            nautilus_batch_rename_dialog_query_finished           (NautilusBatchRenameDialog *dialog,
                                                                       GHashTable                *hash_table,
                                                                       GList                     *selection_metadata);

void            check_conflict_for_files                               (NautilusBatchRenameDialog *dialog,
                                                                        NautilusDirectory         *directory,
                                                                        GList                     *files);

G_END_DECLS

#endif
