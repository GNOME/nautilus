#include <config.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "nautilus-file-operations-admin.h"
#include "nautilus-file-operations-private.h"
#include "nautilus-file-operations-dbus-data.h"
#include "nautilus-ui-utilities.h"
#include "nautilus-file-utilities.h"
#include "nautilus-file.h"

typedef enum
{
    NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_INVALID,
    NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_COPY_TO_DIR,
    NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_MOVE_TO_DIR,
    NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_MOVE_FROM_DIR,
    NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_COPY_FILE,
    NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_MOVE_FILE,
    NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_DELETE_FILE,
} NautilusAdminFileOpPermissionDlgType;

typedef enum
{
    NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_RESPONSE_INVALID,
    NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_RESPONSE_YES,
    NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_RESPONSE_SKIP,
    NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_RESPONSE_SKIP_ALL,
    NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_RESPONSE_CANCEL,
} NautilusAdminFileOpPermissionDlgResponse;


typedef void (*FileOpAutoAdminMountFinishedCallback) (gboolean success,
                                                      gpointer callback_data,
                                                      GError  *error);

typedef struct
{
    FileOpAutoAdminMountFinishedCallback mount_finished_cb;
    gpointer mount_finished_cb_data;
} AdminMountCbData;

typedef struct
{
    gboolean completed;
    gboolean success;
    GMutex mutex;
    GCond cond;
} FileSubopAutoAdminMountData;

typedef struct
{
    NautilusAdminFileOpPermissionDlgType dlg_type;
    GtkWindow *parent_window;
    GFile *file;
    gboolean completed;
    int response;
    GMutex mutex;
    GCond cond;
} AdminOpPermissionDialogData;

static gboolean admin_vfs_mounted = FALSE;

static GFile *
get_as_admin_file (GFile *file)
{
    g_autofree gchar *uri_path = NULL;
    g_autofree gchar *uri = NULL;
    g_autofree char *admin_uri = NULL;
    gboolean uri_op_success;

    if (file == NULL)
    {
        return NULL;
    }

    uri = g_file_get_uri (file);
    uri_op_success = g_uri_split (uri, G_URI_FLAGS_NONE,
                                  NULL, NULL, NULL, NULL,
                                  &uri_path,
                                  NULL, NULL, NULL);

    g_assert (uri_op_success);

    admin_uri = g_strconcat ("admin://", uri_path, NULL);

    return g_file_new_for_uri (admin_uri);
}

static void
file_op_async_auto_admin_vfs_mount_cb (GObject      *source_object,
                                       GAsyncResult *res,
                                       gpointer      user_data)
{
    /*This is only called in thread-default context of main (aka UI) thread*/

    g_autoptr (GError) error = NULL;
    AdminMountCbData *cb_data = user_data;
    gboolean mount_success;

    mount_success = g_file_mount_enclosing_volume_finish (G_FILE (source_object), res, &error);

    if (mount_success || g_error_matches (error, G_IO_ERROR, G_IO_ERROR_ALREADY_MOUNTED))
    {
        admin_vfs_mounted = TRUE;
        mount_success = TRUE;
    }

    cb_data->mount_finished_cb (mount_success, cb_data->mount_finished_cb_data, error);

    g_slice_free (AdminMountCbData, cb_data);
}

static void
mount_admin_vfs_async (FileOpAutoAdminMountFinishedCallback mount_finished_cb,
                       gpointer                             mount_finished_cb_data)
{
    g_autoptr (GFile) admin_root = NULL;
    AdminMountCbData *cb_data;

    cb_data = g_slice_new0 (AdminMountCbData);
    cb_data->mount_finished_cb = mount_finished_cb;
    cb_data->mount_finished_cb_data = mount_finished_cb_data;

    admin_root = g_file_new_for_uri ("admin:///");

    g_file_mount_enclosing_volume (admin_root,
                                   G_MOUNT_MOUNT_NONE,
                                   NULL,
                                   NULL,
                                   file_op_async_auto_admin_vfs_mount_cb,
                                   cb_data);
}

static void
mount_admin_vfs_sync_finished (gboolean  success,
                               gpointer  callback_data,
                               GError   *error)
{
    FileSubopAutoAdminMountData *data = callback_data;

    g_mutex_lock (&data->mutex);

    data->success = success;
    data->completed = TRUE;

    g_cond_signal (&data->cond);
    g_mutex_unlock (&data->mutex);
}

static gboolean
mount_admin_vfs_sync (void)
{
    FileSubopAutoAdminMountData data = { 0 };

    data.completed = FALSE;
    data.success = FALSE;

    g_mutex_init (&data.mutex);
    g_cond_init (&data.cond);

    g_mutex_lock (&data.mutex);

    mount_admin_vfs_async (mount_admin_vfs_sync_finished,
                           &data);

    while (!data.completed)
    {
        g_cond_wait (&data.cond, &data.mutex);
    }

    g_mutex_unlock (&data.mutex);
    g_mutex_clear (&data.mutex);
    g_cond_clear (&data.cond);

    return data.success;
}

static int
do_admin_file_op_permission_dialog (GtkWindow                            *parent_window,
                                    NautilusAdminFileOpPermissionDlgType  dlg_type,
                                    GFile                                *file)
{
    GtkWidget *dialog;
    int response;
    GtkWidget *button;

    if (dlg_type == NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_COPY_TO_DIR ||
        dlg_type == NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_MOVE_TO_DIR ||
        dlg_type == NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_MOVE_FROM_DIR)
    {
        dialog = gtk_message_dialog_new (parent_window,
                                         0,
                                         GTK_MESSAGE_OTHER,
                                         GTK_BUTTONS_NONE,
                                         NULL);

        g_object_set (dialog,
                      "text", _("Destination directory access denied"),
                      "secondary-text", _("You'll need to provide administrator permissions to paste files into this directory."),
                      NULL);

        gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"),
                               NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_RESPONSE_CANCEL);
        button = gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Continue"),
                                        NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_RESPONSE_YES);
        gtk_style_context_add_class (gtk_widget_get_style_context (button),
                                     "suggested-action");

        gtk_dialog_set_default_response (GTK_DIALOG (dialog), NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_RESPONSE_CANCEL);
    }
    else
    {
        g_autofree gchar *secondary_text = NULL;
        g_autofree gchar *basename = NULL;

        basename = get_basename (file);

        switch (dlg_type)
        {
            case NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_COPY_FILE:
            {
                secondary_text = g_strdup_printf (_("You'll need to provide administrator permissions to copy“%s”."), basename);
            }
            break;

            case NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_MOVE_FILE:
            {
                secondary_text = g_strdup_printf (_("You'll need to provide administrator permissions to move “%s”."), basename);
            }
            break;

            case NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_DELETE_FILE:
            {
                secondary_text = g_strdup_printf (_("You'll need to provide administrator permissions to permanently delete “%s”."), basename);
            }
            break;

            default:
            {
                g_return_val_if_reached (NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_RESPONSE_CANCEL);
            }
        }

        dialog = gtk_message_dialog_new (parent_window,
                                         0,
                                         GTK_MESSAGE_QUESTION,
                                         GTK_BUTTONS_NONE,
                                         NULL);

        g_object_set (dialog,
                      "text", _("Insufficient Access"),
                      "secondary-text", secondary_text,
                      NULL);

        /*TODO: Confirm correct pnemonic keys */

        gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"), NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_RESPONSE_CANCEL);
        gtk_dialog_add_button (GTK_DIALOG (dialog), _("Skip _All"), NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_RESPONSE_SKIP_ALL);
        gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Skip"), NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_RESPONSE_SKIP);
        button = gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Continue"), NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_RESPONSE_YES);
        gtk_style_context_add_class (gtk_widget_get_style_context (button),
                                     "suggested-action");

        gtk_dialog_set_default_response (GTK_DIALOG (dialog), NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_RESPONSE_CANCEL);
    }

    response = gtk_dialog_run (GTK_DIALOG (dialog));

    gtk_widget_destroy (dialog);

    return response;
}

static gboolean
do_admin_file_op_permission_dialog2 (gpointer callback_data)
{
    AdminOpPermissionDialogData *data = callback_data;
    int response;

    response = do_admin_file_op_permission_dialog (data->parent_window, data->dlg_type, data->file);

    g_mutex_lock (&data->mutex);

    data->completed = TRUE;
    data->response = response;

    g_cond_signal (&data->cond);
    g_mutex_unlock (&data->mutex);

    return FALSE;
}

static int
ask_permission_for_admin_file_subop_in_main_thread (GtkWindow                            *parent_window,
                                                    NautilusAdminFileOpPermissionDlgType  prompt_type,
                                                    GFile                                *file)
{
    AdminOpPermissionDialogData data = { 0 };

    data.dlg_type = prompt_type;
    data.parent_window = parent_window;
    data.file = file;
    data.completed = FALSE;

    g_mutex_init (&data.mutex);
    g_cond_init (&data.cond);

    g_mutex_lock (&data.mutex);

    g_main_context_invoke (NULL,
                           do_admin_file_op_permission_dialog2,
                           &data);

    while (!data.completed)
    {
        g_cond_wait (&data.cond, &data.mutex);
    }

    g_mutex_unlock (&data.mutex);
    g_mutex_clear (&data.mutex);
    g_cond_clear (&data.cond);

    return data.response;
}

static GFile *
get_admin_file_with_permission (CommonJob *job,
                                GFile     *file)
{
    if (!job->admin_permission_granted && !job->admin_permission_denied)
    {
        int dlg_user_response;
        NautilusAdminFileOpPermissionDlgType prompt_type;

        switch (job->kind)
        {
            case OP_KIND_COPY:
            {
                prompt_type = NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_COPY_FILE;
            }
            break;

            case OP_KIND_MOVE:
            {
                prompt_type = NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_MOVE_FILE;
            }
            break;

            case OP_KIND_DELETE:
            {
                prompt_type = NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_DELETE_FILE;
            }
            break;

            default:
            {
                g_return_val_if_reached (NULL);
            }
        }

        dlg_user_response = ask_permission_for_admin_file_subop_in_main_thread (
            job->parent_window,
            prompt_type,
            file);

        switch (dlg_user_response)
        {
            case NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_RESPONSE_YES:
            {
                job->admin_permission_granted = TRUE;
            }
            break;

            case NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_RESPONSE_SKIP:
            {
            }
            break;

            case NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_RESPONSE_SKIP_ALL:
            {
                job->admin_permission_denied = TRUE;
            }
            break;

            case NAUTILUS_ADMIN_FILE_OP_PERMISSION_DLG_RESPONSE_CANCEL:
            {
                g_cancellable_cancel (job->cancellable);
            }
            break;

            default:
            {
                g_assert_not_reached ();
            }
        }
    }

    if (!job->admin_permission_granted)
    {
        return NULL;
    }

    if (!admin_vfs_mounted)
    {
        if (!mount_admin_vfs_sync ())
        {
            return NULL;
        }
    }

    return get_as_admin_file (file);
}

/* Tailored for use in do-while blocks wrapping a GIO call. */
gboolean
retry_with_admin_uri (GFile     **try_file,
                      CommonJob  *job,
                      GError    **error)
{
    g_autoptr (GFile) admin_file = NULL;

    g_return_val_if_fail (try_file != NULL && G_IS_FILE (*try_file), FALSE);

    /* If an admin:// uri has already been tried, there is no need to try again.
     * If it's not a permission error, or the file is not native, the admin
     * backend is not going to help us. */
    if (g_file_has_uri_scheme (*try_file, "admin") ||
        !g_error_matches (*error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED) ||
        !g_file_is_native (*try_file) ||
        g_cancellable_is_cancelled (job->cancellable))
    {
        return FALSE;
    }

    admin_file = get_admin_file_with_permission (job, *try_file);
    if (admin_file == NULL)
    {
        return FALSE;
    }

    /* Try again once with admin backend. */
    g_set_object (try_file, admin_file);
    g_clear_error (error);
    return TRUE;
}
