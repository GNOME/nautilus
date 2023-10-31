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

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "nautilus-files-view.h"

G_BEGIN_DECLS

typedef enum
{
    EQUIPMENT,
    CREATION_DATE,
    SEASON_NUMBER,
    EPISODE_NUMBER,
    TRACK_NUMBER,
    ARTIST_NAME,
    TITLE,
    ALBUM_NAME,
    ORIGINAL_FILE_NAME,
    METADATA_INVALID,
} MetadataType;

typedef enum
{
    NUMBERING_NO_ZERO_PAD,
    NUMBERING_ONE_ZERO_PAD,
    NUMBERING_TWO_ZERO_PAD,
    NUMBERING_INVALID,
} NumberingType;

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
    const gchar *action_name;
    const gchar *label;
    MetadataType metadata_type;
    NumberingType numbering_type;
    gboolean is_metadata;
} TagConstants;

typedef struct
{
    const gchar *action_target_name;
    const gchar *label;
    const SortMode sort_mode;
} SortConstants;

static const SortConstants sorts_constants[] =
{
    {
        "name-ascending",
        N_("Original Name (Ascending)"),
        ORIGINAL_ASCENDING,
    },
    {
        "name-descending",
        N_("Original Name (Descending)"),
        ORIGINAL_DESCENDING,
    },
    {
        "first-modified",
        N_("First Modified"),
        FIRST_MODIFIED,
    },
    {
        "last-modified",
        N_("Last Modified"),
        LAST_MODIFIED,
    },
    {
        "first-created",
        N_("First Created"),
        FIRST_CREATED,
    },
    {
        "last-created",
        N_("Last Created"),
        LAST_CREATED,
    },
};

static const TagConstants metadata_tags_constants[] =
{
    {
        "add-equipment-tag",
        N_("Camera model"),
        EQUIPMENT,
        NUMBERING_INVALID,
        TRUE,
    },
    {
        "add-creation-date-tag",
        N_("Creation date"),
        CREATION_DATE,
        NUMBERING_INVALID,
        TRUE,
    },
    {
        "add-season-number-tag",
        N_("Season number"),
        SEASON_NUMBER,
        NUMBERING_INVALID,
        TRUE,
    },
    {
        "add-episode-number-tag",
        N_("Episode number"),
        EPISODE_NUMBER,
        NUMBERING_INVALID,
        TRUE,
    },
    {
        "add-track-number-tag",
        N_("Track number"),
        TRACK_NUMBER,
        NUMBERING_INVALID,
        TRUE,
    },
    {
        "add-artist-name-tag",
        N_("Artist name"),
        ARTIST_NAME,
        NUMBERING_INVALID,
        TRUE,
    },
    {
        "add-title-tag",
        N_("Title"),
        TITLE,
        NUMBERING_INVALID,
        TRUE,
    },
    {
        "add-album-name-tag",
        N_("Album name"),
        ALBUM_NAME,
        NUMBERING_INVALID,
        TRUE,
    },
    {
        "add-original-file-name-tag",
        N_("Original file name"),
        ORIGINAL_FILE_NAME,
        NUMBERING_INVALID,
        TRUE,
    },
};

static const TagConstants numbering_tags_constants[] =
{
    {
        "add-numbering-no-zero-pad-tag",
        N_("1, 2, 3"),
        METADATA_INVALID,
        NUMBERING_NO_ZERO_PAD,
        FALSE,
    },
    {
        "add-numbering-one-zero-pad-tag",
        N_("01, 02, 03"),
        METADATA_INVALID,
        NUMBERING_ONE_ZERO_PAD,
        FALSE,
    },
    {
        "add-numbering-two-zero-pad-tag",
        N_("001, 002, 003"),
        METADATA_INVALID,
        NUMBERING_TWO_ZERO_PAD,
        FALSE,
    },
};

typedef struct
{
    gchar *name;
    gint index;
} ConflictData;

typedef struct {
    GString *file_name;
    GString *metadata [G_N_ELEMENTS (metadata_tags_constants)];
} FileMetadata;

#define NAUTILUS_TYPE_BATCH_RENAME_DIALOG (nautilus_batch_rename_dialog_get_type())

G_DECLARE_FINAL_TYPE (NautilusBatchRenameDialog, nautilus_batch_rename_dialog, NAUTILUS, BATCH_RENAME_DIALOG, AdwWindow);

GtkWidget*      nautilus_batch_rename_dialog_new                      (GList                     *selection,
                                                                       NautilusDirectory         *directory,
                                                                       NautilusWindow            *window);

void            nautilus_batch_rename_dialog_query_finished           (NautilusBatchRenameDialog *dialog,
                                                                       GHashTable                *hash_table,
                                                                       GList                     *selection_metadata);

G_END_DECLS
