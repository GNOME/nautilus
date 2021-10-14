#pragma once

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <gnome-autoar/gnome-autoar.h>

#include "nautilus-file-operations.h"
#include "nautilus-file-operations-dbus-data.h"
#include "nautilus-progress-info.h"
#include "nautilus-file-undo-operations.h"

typedef enum
{
    OP_KIND_NOT_SET = 0,
    OP_KIND_COPY,
    OP_KIND_MOVE,
    OP_KIND_DELETE,
    OP_KIND_TRASH,
    OP_KIND_COMPRESS
} OpKind;

typedef struct
{
    OpKind kind;
    GTimer *time;
    GtkWindow *parent_window;
    NautilusFileOperationsDBusData *dbus_data;
    guint inhibit_cookie;
    NautilusProgressInfo *progress;
    GCancellable *cancellable;
    GHashTable *skip_files;
    GHashTable *skip_readdir_error;
    NautilusFileUndoInfo *undo_info;
    gboolean skip_all_error;
    gboolean skip_all_conflict;
    gboolean merge_all;
    gboolean replace_all;
    gboolean delete_all;
} CommonJob;

typedef struct
{
    CommonJob common;
    gboolean is_move;
    GList *files;
    GFile *destination;
    GFile *fake_display_source;
    GHashTable *debuting_files;
    gchar *target_name;
    NautilusCopyCallback done_callback;
    gpointer done_callback_data;
} CopyMoveJob;

typedef struct
{
    CommonJob common;
    GList *files;
    gboolean try_trash;
    gboolean user_cancel;
    NautilusDeleteCallback done_callback;
    gpointer done_callback_data;
} DeleteJob;

typedef struct
{
    CommonJob common;
    GFile *dest_dir;
    char *filename;
    gboolean make_dir;
    GFile *src;
    char *src_data;
    int length;
    GFile *created_file;
    NautilusCreateCallback done_callback;
    gpointer done_callback_data;
} CreateJob;


typedef struct
{
    CommonJob common;
    GList *trash_dirs;
    gboolean should_confirm;
    NautilusOpCallback done_callback;
    gpointer done_callback_data;
} EmptyTrashJob;

typedef struct
{
    CommonJob common;
    GFile *file;
    NautilusOpCallback done_callback;
    gpointer done_callback_data;
    guint32 file_permissions;
    guint32 file_mask;
    guint32 dir_permissions;
    guint32 dir_mask;
} SetPermissionsJob;

typedef struct
{
    CommonJob common;
    GList *source_files;
    GFile *destination_directory;
    GList *output_files;
    gboolean destination_decided;
    gboolean extraction_failed;

    gdouble base_progress;

    guint64 archive_compressed_size;
    guint64 total_compressed_size;
    gint total_files;

    NautilusExtractCallback done_callback;
    gpointer done_callback_data;
} ExtractJob;

typedef struct
{
    CommonJob common;
    GList *source_files;
    GFile *output_file;

    AutoarFormat format;
    AutoarFilter filter;
    gchar *passphrase;

    guint64 total_size;
    guint total_files;

    gboolean success;

    NautilusCreateCallback done_callback;
    gpointer done_callback_data;
} CompressJob;
