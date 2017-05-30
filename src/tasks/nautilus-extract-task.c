#include "nautilus-extract-task.h"

#include "nautilus-file-changes-queue.h"
#include "nautilus-file-task-private.h"
#include "nautilus-file-utilities.h"
#include "nautilus-operations-ui-manager.h"
#include "nautilus-task-private.h"

#include <glib/gi18n.h>
#include <gnome-autoar/gnome-autoar.h>

#define NSEC_PER_MICROSEC 1000
#define PROGRESS_NOTIFY_INTERVAL 100 * NSEC_PER_MICROSEC

struct _NautilusExtractTask
{
    GObject parent_instance;

    GList *source_files;
    GFile *destination_directory;
    GList *output_files;

    gdouble base_progress;

    guint64 archive_compressed_size;
    guint64 total_compressed_size;
};

G_DEFINE_TYPE (NautilusExtractTask, nautilus_extract_task,
               NAUTILUS_TYPE_FILE_TASK)

enum
{
    PROP_SOURCE_FILES = 1,
    PROP_DESTINATION_DIRECTORY,
    N_PROPERTIES
};

enum
{
    COMPLETED,
    LAST_SIGNAL
};

static GParamSpec *properties[N_PROPERTIES] = { NULL };
static guint signals[LAST_SIGNAL] = { 0 };

static gpointer
object_copy_func (gconstpointer src,
                  gpointer      data)
{
    /* We /really/ want the const qualifier gone. */
    return g_object_ref ((gpointer) src);
}

static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
    NautilusExtractTask *task;

    task = NAUTILUS_EXTRACT_TASK (object);

    switch (property_id)
    {
        case PROP_SOURCE_FILES:
        {
            GList *list;

            list = g_value_get_pointer (value);
            task->source_files = g_list_copy_deep (list,
                                                   object_copy_func, NULL);
        }
        break;

        case PROP_DESTINATION_DIRECTORY:
        {
            GObject *object;

            object = g_value_get_pointer (value);

            task->destination_directory = g_object_ref (object);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

static void
finalize (GObject *object)
{
    NautilusExtractTask *self;

    self = NAUTILUS_EXTRACT_TASK (object);

    g_list_free_full (self->source_files, g_object_unref);
    g_list_free_full (self->output_files, g_object_unref);
    g_clear_object (&self->destination_directory);

    nautilus_file_changes_consume_changes (TRUE);

    G_OBJECT_CLASS (nautilus_extract_task_parent_class)->finalize (object);
}

static void
on_scanned (AutoarExtractor *extractor,
            guint            total_files,
            gpointer         user_data)
{
    NautilusFileTask *file_task;
    NautilusProgressInfo *progress;
    NautilusTask *task;
    GCancellable *cancellable;
    guint64 total_size;
    GFile *source_file;
    g_autofree gchar *basename = NULL;
    GFileInfo *fsinfo;
    guint64 free_size;

    file_task = user_data;
    progress = nautilus_file_task_get_progress_info (file_task);
    task = NAUTILUS_TASK (file_task);
    cancellable = nautilus_task_get_cancellable (task);
    total_size = autoar_extractor_get_total_size (extractor);
    source_file = autoar_extractor_get_source_file (extractor);
    basename = nautilus_file_task_get_basename (source_file);

    fsinfo = g_file_query_filesystem_info (source_file,
                                           G_FILE_ATTRIBUTE_FILESYSTEM_FREE ","
                                           G_FILE_ATTRIBUTE_FILESYSTEM_READONLY,
                                           cancellable,
                                           NULL);
    free_size = g_file_info_get_attribute_uint64 (fsinfo,
                                                  G_FILE_ATTRIBUTE_FILESYSTEM_FREE);

    /* FIXME: G_MAXUINT64 is the value used by autoar when the file size cannot
     * be determined. Ideally an API should be used instead.
     */
    if (total_size != G_MAXUINT64 && total_size > free_size)
    {
        nautilus_progress_info_take_status (progress,
                                            g_strdup_printf (_("Error extracting “%s”"),
                                                             basename));
        nautilus_file_task_prompt_error (file_task,
                                         g_strdup_printf (_("Not enough free space to extract %s"), basename),
                                         NULL,
                                         NULL,
                                         FALSE,
                                         CANCEL,
                                         NULL);

        /*nautilus_task_cancel (task);*/
    }
}

static void
on_error (AutoarExtractor *extractor,
          GError          *error,
          gpointer         user_data)
{
    NautilusExtractTask *task;
    NautilusFileTask *file_task;
    GFile *source_file;
    GtkWindow *parent_window;
    gint response_id;
    g_autofree gchar *basename = NULL;
    NautilusProgressInfo *progress;

    task = user_data;
    file_task = NAUTILUS_FILE_TASK (task);
    source_file = autoar_extractor_get_source_file (extractor);
    parent_window = nautilus_file_task_get_parent_window (file_task);
    progress = nautilus_file_task_get_progress_info (file_task);

    if (IS_IO_ERROR (error, NOT_SUPPORTED))
    {
        handle_unsupported_compressed_file (parent_window, source_file);

        return;
    }

    basename = nautilus_file_task_get_basename (source_file);
    nautilus_progress_info_take_status (progress,
                                        g_strdup_printf (_("Error extracting “%s”"),
                                                         basename));

    response_id = nautilus_file_task_prompt_warning (file_task,
                                                    g_strdup_printf (_("There was an error while extracting “%s”."),
                                                                    basename),
                                                    g_strdup (error->message),
                                                    NULL,
                                                    FALSE,
                                                    CANCEL,
                                                    SKIP,
                                                    NULL);

    if (response_id == 0 || response_id == GTK_RESPONSE_DELETE_EVENT)
    {
        /*nautilus_task_cancel (NAUTILUS_TASK (task));*/
    }
}

static GFile *
on_decide_destination (AutoarExtractor *extractor,
                       GFile           *destination,
                       GList           *files,
                       gpointer         user_data)
{
    NautilusExtractTask *task;
    NautilusFileTask *file_task;
    NautilusProgressInfo *progress;
    GFile *decided_destination;
    g_autoptr (GCancellable) cancellable = NULL;
    g_autofree char *basename = NULL;

    task = user_data;
    file_task = NAUTILUS_FILE_TASK (task);
    progress = nautilus_file_task_get_progress_info (file_task);

    nautilus_progress_info_set_details (progress,
                                        _("Verifying destination"));

    basename = g_file_get_basename (destination);
    decided_destination = nautilus_generate_unique_file_in_directory (task->destination_directory,
                                                                      basename);
    cancellable = nautilus_task_get_cancellable (NAUTILUS_TASK (task));

    if (g_cancellable_is_cancelled (cancellable))
    {
        g_object_unref (decided_destination);
        return NULL;
    }

    task->output_files = g_list_prepend (task->output_files,
                                         decided_destination);

    return g_object_ref (decided_destination);
}

static void
on_progress (AutoarExtractor *extractor,
             guint64          archive_current_decompressed_size,
             guint            archive_current_decompressed_files,
             gpointer         user_data)
{
    NautilusExtractTask *task;
    NautilusFileTask *file_task;
    NautilusProgressInfo *progress;
    GFile *source_file;
    char *details;
    double elapsed;
    double transfer_rate;
    int remaining_time;
    guint64 archive_total_decompressed_size;
    gdouble archive_weight;
    gdouble archive_decompress_progress;
    guint64 job_completed_size;
    gdouble job_progress;
    g_autofree gchar *basename = NULL;
    g_autofree gchar *formatted_size_job_completed_size = NULL;
    g_autofree gchar *formatted_size_total_compressed_size = NULL;

    task = user_data;
    file_task = NAUTILUS_FILE_TASK (task);
    progress = nautilus_file_task_get_progress_info (file_task);
    source_file = autoar_extractor_get_source_file (extractor);

    basename = nautilus_file_task_get_basename (source_file);
    nautilus_progress_info_take_status (progress,
                                        g_strdup_printf (_("Extracting “%s”"),
                                                         basename));

    archive_total_decompressed_size = autoar_extractor_get_total_size (extractor);

    archive_decompress_progress = (gdouble) archive_current_decompressed_size /
                                  (gdouble) archive_total_decompressed_size;

    archive_weight = 0;
    if (task->total_compressed_size)
    {
        archive_weight = (gdouble) task->archive_compressed_size /
                         (gdouble) task->total_compressed_size;
    }

    job_progress = archive_decompress_progress * archive_weight + task->base_progress;

    elapsed = nautilus_progress_info_get_total_elapsed_time (progress);

    transfer_rate = 0;
    remaining_time = -1;

    job_completed_size = job_progress * task->total_compressed_size;

    if (elapsed > 0)
    {
        transfer_rate = job_completed_size / elapsed;
    }
    if (transfer_rate > 0)
    {
        remaining_time = (task->total_compressed_size - job_completed_size) /
                         transfer_rate;
    }

    formatted_size_job_completed_size = g_format_size (job_completed_size);
    formatted_size_total_compressed_size = g_format_size (task->total_compressed_size);
    if (elapsed < SECONDS_NEEDED_FOR_RELIABLE_TRANSFER_RATE ||
        transfer_rate == 0)
    {
        /* To translators: %s will expand to a size like "2 bytes" or
         * "3 MB", so something like "4 kb / 4 MB"
         */
        details = g_strdup_printf (_("%s / %s"), formatted_size_job_completed_size,
                                   formatted_size_total_compressed_size);
    }
    else
    {
        g_autofree gchar *formatted_time = NULL;
        g_autofree gchar *formatted_size_transfer_rate = NULL;

        formatted_time = nautilus_file_task_get_formatted_time (remaining_time);
        formatted_size_transfer_rate = g_format_size ((goffset) transfer_rate);
        /* To translators: %s will expand to a size like "2 bytes" or
         * "3 MB", %s to a time duration like "2 minutes". So the whole
         * thing will be something like
         * "2 kb / 4 MB -- 2 hours left (4kb/sec)"
         *
         * The singular/plural form will be used depending on the
         * remaining time (i.e. the %s argument).
         */
        details = g_strdup_printf (ngettext ("%s / %s \xE2\x80\x94 %s left (%s/sec)",
                                             "%s / %s \xE2\x80\x94 %s left (%s/sec)",
                                             nautilus_file_task_seconds_count_format_time_units (remaining_time)),
                                             formatted_size_job_completed_size,
                                             formatted_size_total_compressed_size,
                                             formatted_time,
                                             formatted_size_transfer_rate);
    }

    nautilus_progress_info_take_details (progress, details);

    if (elapsed > SECONDS_NEEDED_FOR_APPROXIMATE_TRANSFER_RATE)
    {
        nautilus_progress_info_set_remaining_time (progress, remaining_time);
        nautilus_progress_info_set_elapsed_time (progress, elapsed);
    }

    nautilus_progress_info_set_progress (progress, job_progress, 1);
}


static void
report_extract_final_progress (NautilusExtractTask *self,
                               gint                 total_files)
{
    NautilusFileTask *file_task;
    NautilusProgressInfo *progress;
    char *status;
    g_autofree gchar *basename_dest = NULL;
    g_autofree gchar *formatted_size = NULL;

    file_task = NAUTILUS_FILE_TASK (self);
    progress = nautilus_file_task_get_progress_info (file_task);

    nautilus_progress_info_set_destination (progress,
                                            self->destination_directory);
    basename_dest = nautilus_file_task_get_basename (self->destination_directory);

    if (total_files == 1)
    {
        GFile *source_file;
        g_autofree gchar * basename = NULL;

        source_file = G_FILE (self->source_files->data);
        basename = nautilus_file_task_get_basename (source_file);
        status = g_strdup_printf (_("Extracted “%s” to “%s”"),
                                  basename,
                                  basename_dest);
    }
    else
    {
        status = g_strdup_printf (ngettext ("Extracted %'d file to “%s”",
                                            "Extracted %'d files to “%s”",
                                            total_files),
                                  total_files,
                                  basename_dest);
    }

    nautilus_progress_info_take_status (progress, status);
    formatted_size = g_format_size (self->total_compressed_size);
    nautilus_progress_info_take_details (progress,
                                         g_strdup_printf (_("%s / %s"),
                                                          formatted_size,
                                                          formatted_size));
}

static void
on_completed (AutoarExtractor *extractor,
              gpointer         user_data)
{
    NautilusExtractTask *task;
    GFile *output_file;

    task = user_data;
    output_file = G_FILE (task->output_files->data);

    nautilus_file_changes_queue_file_added (output_file);
}

static void
execute (NautilusTask *task)
{
    GCancellable *cancellable;
    NautilusFileTask *file_task;
    NautilusProgressInfo *progress;
    NautilusExtractTask *self;
    NautilusFileUndoInfo *undo_info = NULL;
    GList *l;
    GList *existing_output_files = NULL;
    gint total_files;
    g_autofree guint64 *archive_compressed_sizes = NULL;
    gint i;

    cancellable = nautilus_task_get_cancellable (task);
    file_task = NAUTILUS_FILE_TASK (task);
    progress = nautilus_file_task_get_progress_info (file_task);
    self = NAUTILUS_EXTRACT_TASK (task);

    nautilus_file_task_inhibit_power_manager (file_task, _("Extracting Files"));

    if (nautilus_file_undo_manager_is_operating ())
    {
        undo_info =
            nautilus_file_undo_info_extract_new (self->source_files,
                                                 self->destination_directory);
    }

    nautilus_progress_info_set_details (progress,
                                        _("Preparing to extract"));

    total_files = g_list_length (self->source_files);

    archive_compressed_sizes = g_malloc0_n (total_files, sizeof (guint64));
    self->total_compressed_size = 0;

    for (l = self->source_files, i = 0;
         l != NULL && !g_cancellable_is_cancelled (cancellable);
         l = l->next, i++)
    {
        GFile *source_file;
        g_autoptr (GFileInfo) info = NULL;

        source_file = G_FILE (l->data);
        info = g_file_query_info (source_file,
                                  G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                  cancellable,
                                  NULL);

        if (info)
        {
            archive_compressed_sizes[i] = g_file_info_get_size (info);
            self->total_compressed_size += archive_compressed_sizes[i];
        }
    }

    self->base_progress = 0;

    for (l = self->source_files, i = 0;
         l != NULL && !g_cancellable_is_cancelled (cancellable);
         l = l->next, i++)
    {
        g_autoptr (AutoarExtractor) extractor = NULL;

        extractor = autoar_extractor_new (G_FILE (l->data),
                                          self->destination_directory);

        autoar_extractor_set_notify_interval (extractor,
                                              PROGRESS_NOTIFY_INTERVAL);
        g_signal_connect (extractor, "scanned",
                          G_CALLBACK (on_scanned), self);
        g_signal_connect (extractor, "error",
                          G_CALLBACK (on_error), self);
        g_signal_connect (extractor, "decide-destination",
                          G_CALLBACK (on_decide_destination), self);
        g_signal_connect (extractor, "progress",
                          G_CALLBACK (on_progress), self);
        g_signal_connect (extractor, "completed",
                          G_CALLBACK (on_completed), self);

        self->archive_compressed_size = archive_compressed_sizes[i];

        autoar_extractor_start (extractor, cancellable);

        g_signal_handlers_disconnect_by_data (extractor, self);

        self->base_progress += (gdouble) self->archive_compressed_size /
                               (gdouble) self->total_compressed_size;
    }

    if (!g_cancellable_is_cancelled (cancellable))
    {
        report_extract_final_progress (self, total_files);
    }

    for (l = self->output_files; l != NULL; l = l->next)
    {
        GFile *output_file;

        output_file = G_FILE (l->data);

        if (g_file_query_exists (output_file, NULL))
        {
            existing_output_files = g_list_prepend (existing_output_files,
                                                    g_object_ref (output_file));
        }
    }

    g_list_free_full (self->output_files, g_object_unref);

    self->output_files = existing_output_files;

    if (undo_info != NULL)
    {
        if (self->output_files != NULL)
        {
            NautilusFileUndoInfoExtract *extract_undo_info;

            extract_undo_info = NAUTILUS_FILE_UNDO_INFO_EXTRACT (undo_info);

            nautilus_file_undo_info_extract_set_outputs (extract_undo_info,
                                                         self->output_files);

            nautilus_file_undo_manager_set_action (undo_info);
        }

        g_clear_object (&undo_info);
    }

    nautilus_emit_signal_in_main_context (self, signals[COMPLETED],
                                          self->output_files);
}

static void
nautilus_extract_task_class_init (NautilusExtractTaskClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    properties[PROP_SOURCE_FILES] =
        g_param_spec_pointer ("source-files", "Source files", "Source files",
                              G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);

    properties[PROP_DESTINATION_DIRECTORY] =
        g_param_spec_pointer ("destination-directory",
                              "Destination directory", "Destination directory",
                              G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);

    object_class->set_property = set_property;
    object_class->finalize = finalize;

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
    signals[COMPLETED] = g_signal_new ("completed",
                                       NAUTILUS_TYPE_EXTRACT_TASK,
                                       0, 0, NULL, NULL,
                                       g_cclosure_marshal_VOID__POINTER,
                                       G_TYPE_NONE,
                                       1,
                                       G_TYPE_POINTER);

    NAUTILUS_FILE_TASK_CLASS (klass)->execute = execute;
}

static void
nautilus_extract_task_init (NautilusExtractTask *self)
{
}

GList *
nautilus_extract_task_get_output_files (NautilusExtractTask *self)
{
    g_return_val_if_fail (NAUTILUS_IS_EXTRACT_TASK (self), NULL);

    return self->output_files;
}

NautilusTask *
nautilus_extract_task_new (GtkWindow *parent_window,
                           GList     *source_files,
                           GFile     *destination_directory)
{
    return g_object_new (NAUTILUS_TYPE_EXTRACT_TASK,
                         "parent-window", parent_window,
                         "source-files", source_files,
                         "destination-directory", destination_directory,
                         NULL);
}
