
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
} SortingMode;

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
} FileMetadata;

#define NAUTILUS_TYPE_BATCH_RENAME_DIALOG (nautilus_batch_rename_dialog_get_type())

G_DECLARE_FINAL_TYPE (NautilusBatchRenameDialog, nautilus_batch_rename_dialog, NAUTILUS, BATCH_RENAME_DIALOG, GtkDialog);

GtkWidget*      nautilus_batch_rename_dialog_new                      (GList                     *selection,
                                                                       NautilusDirectory         *directory,
                                                                       NautilusWindow            *window);

void            nautilus_batch_rename_dialog_query_finished           (NautilusBatchRenameDialog *dialog,
                                                                       GHashTable                *hash_table,
                                                                       GList                     *selection_metadata);

void            check_conflict_for_file                               (NautilusBatchRenameDialog *dialog,
                                                                       NautilusDirectory         *directory,
                                                                       GList                     *files);

gint            compare_int                                           (gconstpointer              a,
                                                                       gconstpointer              b);

G_END_DECLS

#endif