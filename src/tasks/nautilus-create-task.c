#include "nautilus-create-task.h"

#include "nautilus-file-task-private.h"

#include <gdk/gdk.h>

typedef struct _NautilusCreateTaskPrivate
{
    GFile *dest_dir;
    char *filename;
    gboolean make_dir;
    GFile *src;
    char *src_data;
    int length;
    GdkPoint position;
    gboolean has_position;
    GFile *created_file;

} NautilusCreateTaskPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (NautilusCreateTask, nautilus_create_task,
                                     NAUTILUS_TYPE_FILE_TASK);

static gboolean
execute (NautilusTask  *task,
         GError       **error)
{
    NautilusCreateTask *create_task;
    NautilusCreateTaskPrivate *priv;
    NautilusProgressInfo *progress;
    int count;
    g_autoptr (GFile) dest = NULL;
    g_autofree gchar *dest_uri = NULL;
    char *filename, *filename2, *new_filename;
    char *filename_base, *suffix;
    char *dest_fs_type;
    GError *error;
    gboolean res;
    gboolean filename_is_utf8;
    char *primary, *secondary, *details;
    int response;
    char *data;
    int length;
    GFileOutputStream *out;
    gboolean handled_invalid_filename;
    int max_length, offset;

    create_task = NAUTILUS_CREATE_TASK (task);
    priv = nautilus_create_task_get_instance_private (create_task);
    progress = nautilus_file_task_get_progress_info (NAUTILUS_FILE_TASK (task));

    nautilus_progress_info_start (progress);

    handled_invalid_filename = FALSE;

    dest_fs_type = NULL;
    filename = NULL;

    max_length = get_max_name_length (priv->dest_dir);

    verify_destination (common,
                        priv->dest_dir,
                        NULL, -1);
    if (job_aborted (common))
    {
        goto aborted;
    }

    filename = g_strdup (job->filename);
    filename_is_utf8 = FALSE;
    if (filename)
    {
        filename_is_utf8 = g_utf8_validate (filename, -1, NULL);
    }
    if (filename == NULL)
    {
        if (job->make_dir)
        {
            /* localizers: the initial name of a new folder  */
            filename = g_strdup (_("Untitled Folder"));
            filename_is_utf8 = TRUE;             /* Pass in utf8 */
        }
        else
        {
            if (job->src != NULL)
            {
                g_autofree char *basename = NULL;

                basename = g_file_get_basename (job->src);
                /* localizers: the initial name of a new template document */
                filename = g_strdup_printf ("%s", basename);

                g_free (basename);
            }
            if (filename == NULL)
            {
                /* localizers: the initial name of a new empty document */
                filename = g_strdup (_("Untitled Document"));
                filename_is_utf8 = TRUE;                 /* Pass in utf8 */
            }
        }
    }

    make_file_name_valid_for_dest_fs (filename, dest_fs_type);
    if (filename_is_utf8)
    {
        dest = g_file_get_child_for_display_name (job->dest_dir, filename, NULL);
    }
    if (dest == NULL)
    {
        dest = g_file_get_child (job->dest_dir, filename);
    }
    count = 1;

retry:

    error = NULL;
    if (job->make_dir)
    {
        res = g_file_make_directory (dest,
                                     common->cancellable,
                                     &error);

        if (res)
        {
            GFile *real;

            real = map_possibly_volatile_file_to_real (dest, common->cancellable, &error);
            if (real == NULL)
            {
                res = FALSE;
            }
            else
            {
                g_object_unref (dest);
                dest = real;
            }
        }

        if (res && common->undo_info != NULL)
        {
            nautilus_file_undo_info_create_set_data (NAUTILUS_FILE_UNDO_INFO_CREATE (common->undo_info),
                                                     dest, NULL, 0);
        }
    }
    else
    {
        if (job->src)
        {
            res = g_file_copy (job->src,
                               dest,
                               G_FILE_COPY_NONE,
                               common->cancellable,
                               NULL, NULL,
                               &error);

            if (res)
            {
                GFile *real;

                real = map_possibly_volatile_file_to_real (dest, common->cancellable, &error);
                if (real == NULL)
                {
                    res = FALSE;
                }
                else
                {
                    g_object_unref (dest);
                    dest = real;
                }
            }

            if (res && common->undo_info != NULL)
            {
                gchar *uri;

                uri = g_file_get_uri (job->src);
                nautilus_file_undo_info_create_set_data (NAUTILUS_FILE_UNDO_INFO_CREATE (common->undo_info),
                                                         dest, uri, 0);

                g_free (uri);
            }
        }
        else
        {
            data = "";
            length = 0;
            if (job->src_data)
            {
                data = job->src_data;
                length = job->length;
            }

            out = g_file_create (dest,
                                 G_FILE_CREATE_NONE,
                                 common->cancellable,
                                 &error);
            if (out)
            {
                GFile *real;

                real = map_possibly_volatile_file_to_real_on_write (dest,
                                                                    out,
                                                                    common->cancellable,
                                                                    &error);
                if (real == NULL)
                {
                    res = FALSE;
                    g_object_unref (out);
                }
                else
                {
                    g_object_unref (dest);
                    dest = real;

                    res = g_output_stream_write_all (G_OUTPUT_STREAM (out),
                                                     data, length,
                                                     NULL,
                                                     common->cancellable,
                                                     &error);
                    if (res)
                    {
                        res = g_output_stream_close (G_OUTPUT_STREAM (out),
                                                     common->cancellable,
                                                     &error);

                        if (res && common->undo_info != NULL)
                        {
                            nautilus_file_undo_info_create_set_data (NAUTILUS_FILE_UNDO_INFO_CREATE (common->undo_info),
                                                                     dest, data, length);
                        }
                    }

                    /* This will close if the write failed and we didn't close */
                    g_object_unref (out);
                }
            }
            else
            {
                res = FALSE;
            }
        }
    }

    if (res)
    {
        job->created_file = g_object_ref (dest);
        nautilus_file_changes_queue_file_added (dest);
        dest_uri = g_file_get_uri (dest);
        if (job->has_position)
        {
            nautilus_file_changes_queue_schedule_position_set (dest, job->position, common->screen_num);
        }
        else if (eel_uri_is_desktop (dest_uri))
        {
            nautilus_file_changes_queue_schedule_position_remove (dest);
        }
    }
    else
    {
        g_assert (error != NULL);

        if (IS_IO_ERROR (error, INVALID_FILENAME) &&
            !handled_invalid_filename)
        {
            handled_invalid_filename = TRUE;

            g_assert (dest_fs_type == NULL);
            dest_fs_type = query_fs_type (job->dest_dir, common->cancellable);

            if (count == 1)
            {
                new_filename = g_strdup (filename);
            }
            else
            {
                filename_base = eel_filename_strip_extension (filename);
                offset = strlen (filename_base);
                suffix = g_strdup (filename + offset);

                filename2 = g_strdup_printf ("%s %d%s", filename_base, count, suffix);

                new_filename = NULL;
                if (max_length > 0 && strlen (filename2) > max_length)
                {
                    new_filename = shorten_utf8_string (filename2, strlen (filename2) - max_length);
                }

                if (new_filename == NULL)
                {
                    new_filename = g_strdup (filename2);
                }

                g_free (filename2);
                g_free (suffix);
            }

            if (make_file_name_valid_for_dest_fs (new_filename, dest_fs_type))
            {
                g_clear_object (&dest);

                if (filename_is_utf8)
                {
                    dest = g_file_get_child_for_display_name (job->dest_dir, new_filename, NULL);
                }
                if (dest == NULL)
                {
                    dest = g_file_get_child (job->dest_dir, new_filename);
                }

                g_free (new_filename);
                g_error_free (error);
                goto retry;
            }
            g_free (new_filename);
        }

        if (IS_IO_ERROR (error, EXISTS))
        {
            g_clear_object (&dest);
            filename_base = eel_filename_strip_extension (filename);
            offset = strlen (filename_base);
            suffix = g_strdup (filename + offset);

            filename2 = g_strdup_printf ("%s %d%s", filename_base, ++count, suffix);

            if (max_length > 0 && strlen (filename2) > max_length)
            {
                new_filename = shorten_utf8_string (filename2, strlen (filename2) - max_length);
                if (new_filename != NULL)
                {
                    g_free (filename2);
                    filename2 = new_filename;
                }
            }

            make_file_name_valid_for_dest_fs (filename2, dest_fs_type);
            if (filename_is_utf8)
            {
                dest = g_file_get_child_for_display_name (job->dest_dir, filename2, NULL);
            }
            if (dest == NULL)
            {
                dest = g_file_get_child (job->dest_dir, filename2);
            }
            g_free (filename2);
            g_free (suffix);
            g_error_free (error);
            goto retry;
        }
        else if (IS_IO_ERROR (error, CANCELLED))
        {
            g_error_free (error);
        }
        /* Other error */
        else
        {
            g_autofree gchar *basename = NULL;
            g_autofree gchar *filename = NULL;

            basename = get_basename (dest);
            if (job->make_dir)
            {
                primary = g_strdup_printf (_("Error while creating directory %s."),
                                           basename);
            }
            else
            {
                primary = g_strdup_printf (_("Error while creating file %s."),
                                           basename);
            }
            filename = g_file_get_parse_name (job->dest_dir);
            secondary = g_strdup_printf (_("There was an error creating the directory in %s."),
                                         filename);

            details = error->message;

            response = run_warning (common,
                                    primary,
                                    secondary,
                                    details,
                                    FALSE,
                                    CANCEL, SKIP,
                                    NULL);

            g_error_free (error);

            if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT)
            {
                abort_job (common);
            }
            else if (response == 1)                 /* skip */
            {                   /* do nothing */
            }
            else
            {
                g_assert_not_reached ();
            }
        }
    }

aborted:
    if (dest)
    {
        g_object_unref (dest);
    }
    g_free (filename);
    g_free (dest_fs_type);
}

static void
nautilus_create_task_class_init (NautilusCreateTaskClass *klass)
{
    NAUTILUS_FILE_TASK_CLASS (klass)->execute = execute;
}

static void
nautilus_create_task_init (NautilusCreateTask *self)
{
}
