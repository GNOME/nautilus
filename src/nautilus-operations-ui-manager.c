#include <glib/gi18n.h>

#include "nautilus-operations-ui-manager.h"

#include "nautilus-file.h"
#include "nautilus-file-operations.h"
#include "nautilus-file-conflict-dialog.h"
#include "nautilus-mime-actions.h"
#include "nautilus-program-choosing.h"

typedef struct
{
    GSourceFunc source_func;
    GMutex mutex;
    GCond cond;
    gboolean completed;
} ContextInvokeData;

G_LOCK_DEFINE_STATIC (main_context_sync);

static gboolean
invoke_main_context_source_func_wrapper (gpointer user_data)
{
    ContextInvokeData *data = (ContextInvokeData *) user_data;

    g_mutex_lock (&data->mutex);

    while (data->source_func (user_data))
    {
    }

    return G_SOURCE_REMOVE;
}

static void
invoke_main_context_completed (gpointer user_data)
{
    ContextInvokeData *data = (ContextInvokeData *) user_data;

    data->completed = TRUE;

    g_cond_signal (&data->cond);
    g_mutex_unlock (&data->mutex);
}

/* This function is used to run UI on the main thread in order to ask the user
 * for an action during an operation. Since the operation cannot progress until
 * an action is provided by the user, the current thread needs to be blocked.
 * For this we wait on a condition on the shared data. We proceed further
 * unblocking the thread when invoke_main_context_completed() is called in the
 * UI thread. The user_data pointer must reference a struct whose first member
 * is of type ContextInvokeData.
 */
static void
invoke_main_context_sync (GMainContext *main_context,
                          GSourceFunc   source_func,
                          gpointer      user_data)
{
    ContextInvokeData *data = (ContextInvokeData *) user_data;
    /* Allow only one thread at a time to invoke the main context so we
     * don't get race conditions which could lead to multiple dialogs being
     * displayed at the same time
     */
    G_LOCK (main_context_sync);

    data->source_func = source_func;

    g_mutex_init (&data->mutex);
    g_cond_init (&data->cond);
    data->completed = FALSE;

    g_mutex_lock (&data->mutex);

    g_main_context_invoke (main_context,
                           invoke_main_context_source_func_wrapper,
                           user_data);

    while (!data->completed)
    {
        g_cond_wait (&data->cond, &data->mutex);
    }

    g_mutex_unlock (&data->mutex);

    G_UNLOCK (main_context_sync);

    g_mutex_clear (&data->mutex);
    g_cond_clear (&data->cond);
}

typedef struct
{
    ContextInvokeData parent_type;

    GFile *source_name;
    GFile *destination_name;
    GFile *destination_directory_name;

    gchar *suggestion;

    GtkWindow *parent;

    gboolean should_start_inactive;

    FileConflictResponse *response;

    NautilusFile *source;
    NautilusFile *destination;
    NautilusFile *destination_directory_file;

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
    const char *destination_name;
    const char *destination_directory_name;
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
    GdkPaintable *source_paintable;
    GdkPaintable *destination_paintable;

    destination_paintable = nautilus_file_get_icon_paintable (data->destination,
                                                              NAUTILUS_GRID_ICON_SIZE_SMALL,
                                                              1,
                                                              NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS);

    source_paintable = nautilus_file_get_icon_paintable (data->source,
                                                         NAUTILUS_GRID_ICON_SIZE_SMALL,
                                                         1,
                                                         NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS);

    nautilus_file_conflict_dialog_set_images (data->dialog,
                                              destination_paintable,
                                              source_paintable);

    g_object_unref (destination_paintable);
    g_object_unref (source_paintable);
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
                                                                        "date_modified_with_time");
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
                                                                   "date_modified_with_time");
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
set_conflict_and_suggested_names (FileConflictDialogData *data)
{
    g_autofree gchar *conflict_name = NULL;

    conflict_name = nautilus_file_get_edit_name (data->destination);

    nautilus_file_conflict_dialog_set_conflict_name (data->dialog,
                                                     conflict_name);

    nautilus_file_conflict_dialog_set_suggested_name (data->dialog,
                                                      data->suggestion);
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

    set_conflict_and_suggested_names (data);

    set_replace_button_label (data);

    nautilus_file_monitor_add (data->source, data, NAUTILUS_FILE_ATTRIBUTES_FOR_ICON);
    nautilus_file_monitor_add (data->destination, data, NAUTILUS_FILE_ATTRIBUTES_FOR_ICON);

    data->source_handler_id = g_signal_connect (data->source, "changed",
                                                G_CALLBACK (file_icons_changed), data);
    data->destination_handler_id = g_signal_connect (data->destination, "changed",
                                                     G_CALLBACK (file_icons_changed), data);
}

static void
on_conflict_dialog_closing (GtkWindow *dialog,
                            gpointer   user_data)
{
    FileConflictDialogData *data = user_data;
    ConflictResponse response;

    if (data->handle != NULL)
    {
        nautilus_file_list_cancel_call_when_ready (data->handle);
    }

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

    response = nautilus_file_conflict_dialog_get_response (data->dialog);

    if (response == CONFLICT_RESPONSE_RENAME)
    {
        data->response->new_name =
            nautilus_file_conflict_dialog_get_new_name (data->dialog);
    }
    else if (response != CONFLICT_RESPONSE_CANCEL)
    {
        data->response->apply_to_all =
            nautilus_file_conflict_dialog_get_apply_to_all (data->dialog);
    }

    data->response->id = response;

    gtk_window_destroy (GTK_WINDOW (data->dialog));

    nautilus_file_unref (data->source);
    nautilus_file_unref (data->destination);
    nautilus_file_unref (data->destination_directory_file);

    invoke_main_context_completed (user_data);
}

static gboolean
run_file_conflict_dialog (gpointer user_data)
{
    FileConflictDialogData *data = user_data;
    GList *files = NULL;

    data->source = nautilus_file_get (data->source_name);
    data->destination = nautilus_file_get (data->destination_name);
    data->destination_directory_file = nautilus_file_get (data->destination_directory_name);

    data->dialog = nautilus_file_conflict_dialog_new (data->parent);

    if (data->should_start_inactive)
    {
        nautilus_file_conflict_dialog_delay_buttons_activation (data->dialog);
    }

    files = g_list_prepend (files, data->source);
    files = g_list_prepend (files, data->destination);
    files = g_list_prepend (files, data->destination_directory_file);

    nautilus_file_list_call_when_ready (files,
                                        NAUTILUS_FILE_ATTRIBUTES_FOR_ICON | NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT,
                                        &data->handle,
                                        data->on_file_list_ready,
                                        data);

    g_signal_connect (data->dialog, "close-request", G_CALLBACK (on_conflict_dialog_closing), data);
    gtk_window_present (GTK_WINDOW (data->dialog));

    g_list_free (files);

    return G_SOURCE_REMOVE;
}

FileConflictResponse *
copy_move_conflict_ask_user_action (GtkWindow *parent_window,
                                    gboolean   should_start_inactive,
                                    GFile     *source_name,
                                    GFile     *destination_name,
                                    GFile     *destination_directory_name,
                                    gchar     *suggestion)
{
    FileConflictDialogData *data;
    FileConflictResponse *response;

    data = g_slice_new0 (FileConflictDialogData);
    data->parent = parent_window;
    data->should_start_inactive = should_start_inactive;
    data->source_name = source_name;
    data->destination_name = destination_name;
    data->destination_directory_name = destination_directory_name;
    data->suggestion = suggestion;

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
    ContextInvokeData parent_type;
    GtkWindow *parent_window;
    NautilusFile *file;
} HandleUnsupportedFileData;

static void
on_app_chooser_response (GtkDialog *dialog,
                         gint       response_id,
                         gpointer   user_data)
{
    HandleUnsupportedFileData *data = user_data;
    g_autoptr (GAppInfo) application = NULL;

    if (response_id == GTK_RESPONSE_OK)
    {
        application = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (dialog));
    }

    gtk_window_destroy (GTK_WINDOW (dialog));

    if (application != NULL)
    {
        GList files = {data->file, NULL, NULL};
        nautilus_launch_application (application, &files, data->parent_window);
    }

    invoke_main_context_completed (user_data);
}

static gboolean
open_file_in_application (gpointer user_data)
{
    HandleUnsupportedFileData *data;
    g_autofree gchar *mime_type = NULL;
    GtkWidget *dialog;
    const char *heading;

    data = user_data;
    mime_type = nautilus_file_get_mime_type (data->file);
    dialog = gtk_app_chooser_dialog_new_for_content_type (data->parent_window,
                                                          GTK_DIALOG_MODAL |
                                                          GTK_DIALOG_DESTROY_WITH_PARENT |
                                                          GTK_DIALOG_USE_HEADER_BAR,
                                                          mime_type);
    heading = _("Password-protected archives are not yet supported. "
                "This list contains apps that can open the archive.");

    gtk_app_chooser_dialog_set_heading (GTK_APP_CHOOSER_DIALOG (dialog), heading);

    g_signal_connect (dialog, "response", G_CALLBACK (on_app_chooser_response), data);
    gtk_window_present (GTK_WINDOW (dialog));

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

typedef struct
{
    ContextInvokeData parent_type;
    GtkWindow *parent_window;
    const gchar *basename;
    GtkPasswordEntry *passphrase_entry;
    gchar *passphrase;
} PassphraseRequestData;

static void
on_request_passphrase_cb (AdwMessageDialog *dialog,
                          gchar            *response,
                          gpointer          user_data)
{
    PassphraseRequestData *data = user_data;

    if (g_str_equal (response, "extract"))
    {
        data->passphrase = g_strdup (gtk_editable_get_text (GTK_EDITABLE (data->passphrase_entry)));
    }

    invoke_main_context_completed (data);
}

static gboolean
run_passphrase_dialog (gpointer user_data)
{
    PassphraseRequestData *data = user_data;
    g_autofree gchar *label_str = NULL;
    g_autoptr (GtkBuilder) builder = NULL;
    GObject *dialog;

    builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-operations-ui-manager-request-passphrase.ui");
    dialog = gtk_builder_get_object (builder, "request_passphrase_dialog");
    data->passphrase_entry = GTK_PASSWORD_ENTRY (gtk_builder_get_object (builder, "entry"));

    adw_message_dialog_format_body (ADW_MESSAGE_DIALOG (dialog),
                                    _("“%s” is password-protected."),
                                    data->basename);

    g_signal_connect (dialog, "response", G_CALLBACK (on_request_passphrase_cb), data);
    gtk_window_set_transient_for (GTK_WINDOW (dialog), data->parent_window);
    gtk_window_present (GTK_WINDOW (dialog));

    return G_SOURCE_REMOVE;
}

gchar *
extract_ask_passphrase (GtkWindow   *parent_window,
                        const gchar *archive_basename)
{
    PassphraseRequestData *data;
    gchar *passphrase;

    data = g_new0 (PassphraseRequestData, 1);
    data->parent_window = parent_window;
    data->basename = archive_basename;
    invoke_main_context_sync (NULL, run_passphrase_dialog, data);

    passphrase = g_steal_pointer (&data->passphrase);
    g_free (data);

    return passphrase;
}
