#include <glib/gi18n.h>

#include "nautilus-operations-ui-manager.h"

#include "eel/eel-vfs-extensions.h"

#include "nautilus-file.h"
#include "nautilus-directory.h"
#include "nautilus-file-operations.h"
#include "nautilus-file-conflict-dialog.h"
#include "nautilus-mime-actions.h"
#include "nautilus-program-choosing.h"

typedef struct
{
    GSourceFunc source_func;
    gpointer user_data;
    GMutex mutex;
    GCond cond;
    gboolean completed;
} ContextInvokeData;

G_LOCK_DEFINE_STATIC (main_context_sync);

static gboolean
invoke_main_context_source_func_wrapper (gpointer user_data)
{
    ContextInvokeData *data = user_data;

    g_mutex_lock (&data->mutex);

    while (data->source_func (data->user_data))
    {
    }

    data->completed = TRUE;

    g_cond_signal (&data->cond);
    g_mutex_unlock (&data->mutex);

    return G_SOURCE_REMOVE;
}

/* This function is used to run UI on the main thread in order to ask the user
 * for an action during an operation. Since the operation cannot progress until
 * an action is provided by the user, the current thread needs to be blocked.
 * For this we wait on a condition on the shared data. We proceed further
 * unblocking the thread when the condition is set in the UI thread.
 */
static void
invoke_main_context_sync (GMainContext *main_context,
                          GSourceFunc   source_func,
                          gpointer      user_data)
{
    ContextInvokeData data;
    /* Allow only one thread at a time to invoke the main context so we
     * don't get race conditions which could lead to multiple dialogs being
     * displayed at the same time
     */
    G_LOCK (main_context_sync);

    data.source_func = source_func;
    data.user_data = user_data;

    g_mutex_init (&data.mutex);
    g_cond_init (&data.cond);
    data.completed = FALSE;

    g_mutex_lock (&data.mutex);

    g_main_context_invoke (main_context,
                           invoke_main_context_source_func_wrapper,
                           &data);

    while (!data.completed)
    {
        g_cond_wait (&data.cond, &data.mutex);
    }

    g_mutex_unlock (&data.mutex);

    G_UNLOCK (main_context_sync);

    g_mutex_clear (&data.mutex);
    g_cond_clear (&data.cond);
}

typedef struct
{
    GFile *source_name;
    GFile *destination_name;
    GFile *destination_directory_name;

    GtkWindow *parent;

    FileConflictResponse *response;

    NautilusFile *source;
    NautilusFile *destination;
    NautilusFile *destination_directory_file;

    NautilusDirectory *destination_directory;

    NautilusFileConflictDialog *dialog;

    NautilusFileListCallback on_file_list_ready;
    NautilusFileListHandle *handle;
    gulong source_handler_id;
    gulong destination_handler_id;
} FileConflictDialogData;

void
file_conflict_response_free (FileConflictResponse *response)
{
    g_free (response->new_name);
    g_slice_free (FileConflictResponse, response);
}

static void
set_copy_move_dialog_text (FileConflictDialogData *data)
{
    g_autofree gchar *primary_text = NULL;
    g_autofree gchar *secondary_text = NULL;
    const gchar *message_extra;
    time_t source_mtime;
    time_t destination_mtime;
    g_autofree gchar *message = NULL;
    g_autofree gchar *destination_name = NULL;
    g_autofree gchar *destination_directory_name = NULL;
    gboolean source_is_directory;
    gboolean destination_is_directory;

    source_mtime = nautilus_file_get_mtime (data->source);
    destination_mtime = nautilus_file_get_mtime (data->destination);

    destination_name = nautilus_file_get_display_name (data->destination);
    destination_directory_name = nautilus_file_get_display_name (data->destination_directory_file);

    source_is_directory = nautilus_file_is_directory (data->source);
    destination_is_directory = nautilus_file_is_directory (data->destination);

    if (destination_is_directory)
    {
        if (nautilus_file_is_symbolic_link (data->source)
            && !nautilus_file_is_symbolic_link (data->destination))
        {
            primary_text = g_strdup_printf (_("You are trying to replace the destination folder “%s” with a symbolic link."),
                                            destination_name);
            message = g_strdup_printf (_("This is not allowed in order to avoid the deletion of the destination folder’s contents."));
            message_extra = _("Please rename the symbolic link or press the skip button.");
        }
        else if (source_is_directory)
        {
            primary_text = g_strdup_printf (_("Merge folder “%s”?"),
                                            destination_name);

            message_extra = _("Merging will ask for confirmation before replacing any files in "
                              "the folder that conflict with the files being copied.");

            if (source_mtime > destination_mtime)
            {
                message = g_strdup_printf (_("An older folder with the same name already exists in “%s”."),
                                           destination_directory_name);
            }
            else if (source_mtime < destination_mtime)
            {
                message = g_strdup_printf (_("A newer folder with the same name already exists in “%s”."),
                                           destination_directory_name);
            }
            else
            {
                message = g_strdup_printf (_("Another folder with the same name already exists in “%s”."),
                                           destination_directory_name);
            }
        }
        else
        {
            primary_text = g_strdup_printf (_("Replace folder “%s”?"),
                                            destination_name);
            message_extra = _("Replacing it will remove all files in the folder.");
            message = g_strdup_printf (_("A folder with the same name already exists in “%s”."),
                                       destination_directory_name);
        }
    }
    else
    {
        primary_text = g_strdup_printf (_("Replace file “%s”?"),
                                        destination_name);

        message_extra = _("Replacing it will overwrite its content.");

        if (source_mtime > destination_mtime)
        {
            message = g_strdup_printf (_("An older file with the same name already exists in “%s”."),
                                       destination_directory_name);
        }
        else if (source_mtime < destination_mtime)
        {
            message = g_strdup_printf (_("A newer file with the same name already exists in “%s”."),
                                       destination_directory_name);
        }
        else
        {
            message = g_strdup_printf (_("Another file with the same name already exists in “%s”."),
                                       destination_directory_name);
        }
    }

    secondary_text = g_strdup_printf ("%s\n%s", message, message_extra);

    nautilus_file_conflict_dialog_set_text (data->dialog,
                                            primary_text,
                                            secondary_text);
}

static void
set_images (FileConflictDialogData *data)
{
    GdkPixbuf *source_pixbuf;
    GdkPixbuf *destination_pixbuf;

    destination_pixbuf = nautilus_file_get_icon_pixbuf (data->destination,
                                                        NAUTILUS_CANVAS_ICON_SIZE_SMALL,
                                                        TRUE,
                                                        1,
                                                        NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS);

    source_pixbuf = nautilus_file_get_icon_pixbuf (data->source,
                                                   NAUTILUS_CANVAS_ICON_SIZE_SMALL,
                                                   TRUE,
                                                   1,
                                                   NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS);

    nautilus_file_conflict_dialog_set_images (data->dialog,
                                              destination_pixbuf,
                                              source_pixbuf);

    g_object_unref (destination_pixbuf);
    g_object_unref (source_pixbuf);
}

static void
set_file_labels (FileConflictDialogData *data)
{
    GString *destination_label;
    GString *source_label;
    gboolean source_is_directory;
    gboolean destination_is_directory;
    gboolean should_show_type;
    g_autofree char *destination_mime_type = NULL;
    g_autofree char *destination_date = NULL;
    g_autofree char *destination_size = NULL;
    g_autofree char *destination_type = NULL;
    g_autofree char *source_date = NULL;
    g_autofree char *source_size = NULL;
    g_autofree char *source_type = NULL;

    source_is_directory = nautilus_file_is_directory (data->source);
    destination_is_directory = nautilus_file_is_directory (data->destination);

    destination_mime_type = nautilus_file_get_mime_type (data->destination);
    should_show_type = !nautilus_file_is_mime_type (data->source,
                                                    destination_mime_type);

    destination_date = nautilus_file_get_string_attribute_with_default (data->destination,
                                                                        "date_modified");
    destination_size = nautilus_file_get_string_attribute_with_default (data->destination,
                                                                        "size");

    if (should_show_type)
    {
        destination_type = nautilus_file_get_string_attribute_with_default (data->destination,
                                                                            "type");
    }

    destination_label = g_string_new (NULL);
    if (destination_is_directory)
    {
        g_string_append_printf (destination_label, "<b>%s</b>\n", _("Original folder"));
        g_string_append_printf (destination_label, "%s %s\n", _("Contents:"), destination_size);
    }
    else
    {
        g_string_append_printf (destination_label, "<b>%s</b>\n", _("Original file"));
        g_string_append_printf (destination_label, "%s %s\n", _("Size:"), destination_size);
    }

    if (should_show_type)
    {
        g_string_append_printf (destination_label, "%s %s\n", _("Type:"), destination_type);
    }

    g_string_append_printf (destination_label, "%s %s", _("Last modified:"), destination_date);

    source_date = nautilus_file_get_string_attribute_with_default (data->source,
                                                                   "date_modified");
    source_size = nautilus_file_get_string_attribute_with_default (data->source,
                                                                   "size");

    if (should_show_type)
    {
        source_type = nautilus_file_get_string_attribute_with_default (data->source,
                                                                       "type");
    }

    source_label = g_string_new (NULL);
    if (source_is_directory)
    {
        g_string_append_printf (source_label, "<b>%s</b>\n",
                                destination_is_directory ?
                                _("Merge with") : _("Replace with"));
        g_string_append_printf (source_label, "%s %s\n", _("Contents:"), source_size);
    }
    else
    {
        g_string_append_printf (source_label, "<b>%s</b>\n", _("Replace with"));
        g_string_append_printf (source_label, "%s %s\n", _("Size:"), source_size);
    }

    if (should_show_type)
    {
        g_string_append_printf (source_label, "%s %s\n", _("Type:"), source_type);
    }

    g_string_append_printf (source_label, "%s %s", _("Last modified:"), source_date);

    nautilus_file_conflict_dialog_set_file_labels (data->dialog,
                                                   destination_label->str,
                                                   source_label->str);

    g_string_free (destination_label, TRUE);
    g_string_free (source_label, TRUE);
}

static void
set_conflict_name (FileConflictDialogData *data)
{
    g_autofree gchar *edit_name = NULL;

    edit_name = nautilus_file_get_edit_name (data->destination);

    nautilus_file_conflict_dialog_set_conflict_name (data->dialog,
                                                     edit_name);
}

static void
set_replace_button_label (FileConflictDialogData *data)
{
    gboolean source_is_directory, destination_is_directory;

    source_is_directory = nautilus_file_is_directory (data->source);
    destination_is_directory = nautilus_file_is_directory (data->destination);

    if (destination_is_directory)
    {
        if (nautilus_file_is_symbolic_link (data->source)
            && !nautilus_file_is_symbolic_link (data->destination))
        {
            nautilus_file_conflict_dialog_disable_replace (data->dialog);
            nautilus_file_conflict_dialog_disable_apply_to_all (data->dialog);
        }
        else if (source_is_directory)
        {
            nautilus_file_conflict_dialog_set_replace_button_label (data->dialog,
                                                                    _("Merge"));
        }
    }
}

static void
file_icons_changed (NautilusFile           *file,
                    FileConflictDialogData *data)
{
    set_images (data);
}

static void
copy_move_conflict_on_directory_info_ready (NautilusDirectory *destination_directory,
                                            GList             *file_list,
                                            gpointer           user_data)
{
    FileConflictDialogData *data = user_data;
    g_autolist (NautilusFile) files = NULL;
    g_autofree gchar *destination_edit_name = NULL;
    g_autofree gchar *filename_base = NULL;
    g_autofree gchar *extension = NULL;
    g_autofree gchar *suggested_name = NULL;
    gint count;

    files = nautilus_directory_get_file_list (destination_directory);
    destination_edit_name = nautilus_file_get_edit_name (data->destination);
    filename_base = eel_filename_strip_extension (destination_edit_name);
    extension = g_strdup (destination_edit_name + strlen (filename_base));

    count = 0;

    while (count < G_MAXINT)
    {
        g_autofree gchar *count_string = NULL;
        g_autofree gchar *test_name = NULL;
        gboolean conflict_found;

        count++;

        count_string = g_strdup_printf (" (%d)", count);
        test_name = g_strconcat (filename_base, count_string, extension, NULL);

        conflict_found = FALSE;
        for (GList *l = files; l != NULL; l = l->next)
        {
            g_autofree gchar *file_name = NULL;

            file_name = nautilus_file_get_display_name (l->data);

            if (g_strcmp0 (file_name, test_name) == 0)
            {
                conflict_found = TRUE;
                break;
            }
        }

        if (!conflict_found)
        {
            suggested_name = g_steal_pointer (&test_name);
            break;
        }
    }

    nautilus_file_conflict_dialog_set_suggested_name (data->dialog,
                                                      suggested_name);
}

static void
copy_move_conflict_on_file_list_ready (GList    *files,
                                       gpointer  user_data)
{
    FileConflictDialogData *data = user_data;
    g_autofree gchar *title = NULL;

    data->handle = NULL;

    if (nautilus_file_is_directory (data->source))
    {
        title = g_strdup (nautilus_file_is_directory (data->destination) ?
                          _("Merge Folder") :
                          _("File and Folder conflict"));
    }
    else
    {
        title = g_strdup (nautilus_file_is_directory (data->destination) ?
                          _("File and Folder conflict") :
                          _("File conflict"));
    }

    gtk_window_set_title (GTK_WINDOW (data->dialog), title);

    set_copy_move_dialog_text (data);

    set_images (data);

    set_file_labels (data);

    set_conflict_name (data);

    set_replace_button_label (data);

    nautilus_file_monitor_add (data->source, data, NAUTILUS_FILE_ATTRIBUTES_FOR_ICON);
    nautilus_file_monitor_add (data->destination, data, NAUTILUS_FILE_ATTRIBUTES_FOR_ICON);

    data->source_handler_id = g_signal_connect (data->source, "changed",
                                                G_CALLBACK (file_icons_changed), data);
    data->destination_handler_id = g_signal_connect (data->destination, "changed",
                                                     G_CALLBACK (file_icons_changed), data);

    nautilus_directory_call_when_ready (data->destination_directory,
                                        NAUTILUS_FILE_ATTRIBUTES_FOR_ICON,
                                        TRUE,
                                        copy_move_conflict_on_directory_info_ready,
                                        data);
}

static gboolean
run_file_conflict_dialog (gpointer user_data)
{
    FileConflictDialogData *data = user_data;
    int response_id;
    GList *files = NULL;

    data->source = nautilus_file_get (data->source_name);
    data->destination = nautilus_file_get (data->destination_name);
    data->destination_directory_file = nautilus_file_get (data->destination_directory_name);
    data->destination_directory = nautilus_directory_get (data->destination_directory_name);

    data->dialog = nautilus_file_conflict_dialog_new (data->parent);

    files = g_list_prepend (files, data->source);
    files = g_list_prepend (files, data->destination);
    files = g_list_prepend (files, data->destination_directory_file);

    nautilus_file_list_call_when_ready (files,
                                        NAUTILUS_FILE_ATTRIBUTES_FOR_ICON | NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT,
                                        &data->handle,
                                        data->on_file_list_ready,
                                        data);

    response_id = gtk_dialog_run (GTK_DIALOG (data->dialog));

    if (data->handle != NULL)
    {
        nautilus_file_list_cancel_call_when_ready (data->handle);
    }

    /* Cancel the callback added by on_file_list_ready() */
    nautilus_directory_cancel_callback (data->destination_directory,
                                        copy_move_conflict_on_directory_info_ready,
                                        data);

    if (data->source_handler_id)
    {
        g_signal_handler_disconnect (data->source, data->source_handler_id);
        nautilus_file_monitor_remove (data->source, data);
    }

    if (data->destination_handler_id)
    {
        g_signal_handler_disconnect (data->destination, data->destination_handler_id);
        nautilus_file_monitor_remove (data->destination, data);
    }

    if (response_id == CONFLICT_RESPONSE_RENAME)
    {
        data->response->new_name =
            nautilus_file_conflict_dialog_get_new_name (data->dialog);
    }
    else if (response_id != GTK_RESPONSE_CANCEL &&
             response_id != GTK_RESPONSE_NONE)
    {
        data->response->apply_to_all =
            nautilus_file_conflict_dialog_get_apply_to_all (data->dialog);
    }

    data->response->id = response_id;

    gtk_widget_destroy (GTK_WIDGET (data->dialog));

    nautilus_file_unref (data->source);
    nautilus_file_unref (data->destination);
    nautilus_file_unref (data->destination_directory_file);
    nautilus_directory_unref (data->destination_directory);
    g_list_free (files);

    return G_SOURCE_REMOVE;
}

FileConflictResponse *
copy_move_conflict_ask_user_action (GtkWindow *parent_window,
                                    GFile     *source_name,
                                    GFile     *destination_name,
                                    GFile     *destination_directory_name)
{
    FileConflictDialogData *data;
    FileConflictResponse *response;

    data = g_slice_new0 (FileConflictDialogData);
    data->parent = parent_window;
    data->source_name = source_name;
    data->destination_name = destination_name;
    data->destination_directory_name = destination_directory_name;

    data->response = g_slice_new0 (FileConflictResponse);
    data->response->new_name = NULL;

    data->on_file_list_ready = copy_move_conflict_on_file_list_ready;

    invoke_main_context_sync (NULL,
                              run_file_conflict_dialog,
                              data);

    response = g_steal_pointer (&data->response);
    g_slice_free (FileConflictDialogData, data);

    return response;
}

typedef struct
{
    GtkWindow *parent_window;
    NautilusFile *file;
} HandleUnsupportedFileData;

static gboolean
open_file_in_application (gpointer user_data)
{
    HandleUnsupportedFileData *data;
    g_autofree gchar *mime_type = NULL;
    GtkWidget *dialog;
    const char *heading;
    g_autoptr (GAppInfo) application = NULL;

    data = user_data;
    mime_type = nautilus_file_get_mime_type (data->file);
    dialog = gtk_app_chooser_dialog_new_for_content_type (data->parent_window,
                                                          GTK_DIALOG_MODAL |
                                                          GTK_DIALOG_DESTROY_WITH_PARENT |
                                                          GTK_DIALOG_USE_HEADER_BAR,
                                                          mime_type);
    heading = _("Password-protected archives are not yet supported. "
                "This list contains applications that can open the archive.");

    gtk_app_chooser_dialog_set_heading (GTK_APP_CHOOSER_DIALOG (dialog), heading);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
        application = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (dialog));
    }

    gtk_widget_destroy (dialog);

    if (application != NULL)
    {
        g_autoptr (GList) files = NULL;

        files = g_list_append (NULL, data->file);

        nautilus_launch_application (application, files, data->parent_window);
    }

    return G_SOURCE_REMOVE;
}

/* This is used to open compressed files that are not supported by gnome-autoar
 * in another application
 */
void
handle_unsupported_compressed_file (GtkWindow *parent_window,
                                    GFile     *compressed_file)
{
    HandleUnsupportedFileData *data;

    data = g_slice_new0 (HandleUnsupportedFileData);
    data->parent_window = parent_window;
    data->file = nautilus_file_get (compressed_file);

    invoke_main_context_sync (NULL, open_file_in_application, data);

    nautilus_file_unref (data->file);
    g_slice_free (HandleUnsupportedFileData, data);

    return;
}
