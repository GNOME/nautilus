/* Copyright (C) 2017 Ernestas Kulik <ernestask@gnome.org>
 *
 * This file is part of Nautilus.
 *
 * Nautilus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nautilus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "nautilus-file-task.h"

#include "nautilus-file-task-private.h"
#include "nautilus-file-utilities.h"

#include <eel/eel-string.h>
#include <eel/eel-gtk-extensions.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

typedef struct
{
    GtkWindow *parent_window;
    int screen_num;
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
} NautilusFileTaskPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (NautilusFileTask, nautilus_file_task,
                                  NAUTILUS_TYPE_TASK,
                                  G_ADD_PRIVATE (NautilusFileTask))

enum
{
    PROP_PARENT_WINDOW = 1,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL };

static void
execute (NautilusTask *task)
{
    NautilusFileTask *file_task;
    NautilusFileTaskPrivate *priv;

    file_task = NAUTILUS_FILE_TASK (task);
    priv = nautilus_file_task_get_instance_private (file_task);

    nautilus_progress_info_start (priv->progress);

    NAUTILUS_FILE_TASK_CLASS (G_OBJECT_GET_CLASS (task))->execute (task);

    nautilus_progress_info_finish (priv->progress);
}

static GObject *
constructor (GType                  type,
             guint                  n_construct_properties,
             GObjectConstructParam *construct_properties)
{
    GObjectClass *parent_class;
    GObject *instance;
    NautilusFileTask *self;
    NautilusFileTaskPrivate *priv;

    parent_class = G_OBJECT_CLASS (nautilus_file_task_parent_class);
    instance = parent_class->constructor (type,
                                          n_construct_properties,
                                          construct_properties);
    self = NAUTILUS_FILE_TASK (instance);
    priv = nautilus_file_task_get_instance_private (self);

    priv->progress = nautilus_progress_info_new ();
    priv->cancellable = nautilus_progress_info_get_cancellable (priv->progress);

    g_object_set (instance, "cancellable", priv->cancellable, NULL);

    return instance;
}

static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
    switch (property_id)
    {
        case PROP_PARENT_WINDOW:
        {
            NautilusFileTask *task;
            NautilusFileTaskPrivate *priv;

            task = NAUTILUS_FILE_TASK (object);
            priv = nautilus_file_task_get_instance_private (task);

            priv->parent_window = g_value_get_pointer (value);

            g_object_add_weak_pointer (G_OBJECT (priv->parent_window),
                                       (gpointer *) &priv->parent_window);
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
    NautilusFileTask *task;
    NautilusFileTaskPrivate *priv;

    task = NAUTILUS_FILE_TASK (object);
    priv = nautilus_file_task_get_instance_private (task);

    if (priv->inhibit_cookie != 0)
    {
        gtk_application_uninhibit (GTK_APPLICATION (g_application_get_default ()),
                                   priv->inhibit_cookie);
    }

    priv->inhibit_cookie = 0;

    if (priv->parent_window != NULL)
    {
        g_object_remove_weak_pointer (G_OBJECT (priv->parent_window),
                                      (gpointer *) &priv->parent_window);
    }

    if (priv->skip_files != NULL)
    {
        g_hash_table_destroy (priv->skip_files);
    }
    if (priv->skip_readdir_error != NULL)
    {
        g_hash_table_destroy (priv->skip_readdir_error);
    }

    g_object_unref (priv->progress);
    g_object_unref (priv->cancellable);

    G_OBJECT_CLASS (nautilus_file_task_parent_class)->finalize (object);
}

static void
nautilus_file_task_class_init (NautilusFileTaskClass *klass)
{
    GObjectClass *object_class;
    NautilusTaskClass *task_class;

    object_class = G_OBJECT_CLASS (klass);
    task_class = NAUTILUS_TASK_CLASS (klass);

    object_class->constructor = constructor;
    object_class->set_property = set_property;
    object_class->finalize = finalize;

    task_class->execute = execute;

    properties[PROP_PARENT_WINDOW] =
        g_param_spec_pointer ("parent-window", "Parent window", "Parent window",
                              G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
nautilus_file_task_init (NautilusFileTask *self)
{
    NautilusFileTaskPrivate *priv;

    priv = nautilus_file_task_get_instance_private (self);

    priv->inhibit_cookie = 0;
}

GtkWindow *
nautilus_file_task_get_parent_window (NautilusFileTask *self)
{
    NautilusFileTaskPrivate *priv;

    g_return_val_if_fail (NAUTILUS_IS_FILE_TASK (self), NULL);

    priv = nautilus_file_task_get_instance_private (self);

    return priv->parent_window;
}

NautilusProgressInfo *
nautilus_file_task_get_progress_info (NautilusFileTask *self)
{
    NautilusFileTaskPrivate *priv;

    g_return_val_if_fail (NAUTILUS_IS_FILE_TASK (self), NULL);

    priv = nautilus_file_task_get_instance_private (self);

    return priv->progress;
}

#define MAXIMUM_DISPLAYED_FILE_NAME_LENGTH 50

typedef struct
{
    GtkWindow **parent_window;
    gboolean ignore_close_box;
    GtkMessageType message_type;
    const char *primary_text;
    const char *secondary_text;
    const char *details_text;
    const char **button_titles;
    gboolean show_all;
    int result;
    /* Dialogs are ran from operation threads, which need to be blocked until
     * the user gives a valid response
     */
    gboolean completed;
    GMutex mutex;
    GCond cond;
} RunSimpleDialogData;

static gboolean
is_all_button_text (const char *button_text)
{
    g_assert (button_text != NULL);

    return !strcmp (button_text, SKIP_ALL) ||
           !strcmp (button_text, REPLACE_ALL) ||
           !strcmp (button_text, DELETE_ALL) ||
           !strcmp (button_text, MERGE_ALL);
}


static gboolean
do_run_simple_dialog (gpointer _data)
{
    RunSimpleDialogData *data = _data;
    const char *button_title;
    GtkWidget *dialog;
    GtkWidget *button;
    int result;
    int response_id;

    g_mutex_lock (&data->mutex);

    /* Create the dialog. */
    dialog = gtk_message_dialog_new (*data->parent_window,
                                     0,
                                     data->message_type,
                                     GTK_BUTTONS_NONE,
                                     NULL);

    g_object_set (dialog,
                  "text", data->primary_text,
                  "secondary-text", data->secondary_text,
                  NULL);

    for (response_id = 0;
         data->button_titles[response_id] != NULL;
         response_id++)
    {
        button_title = data->button_titles[response_id];
        if (!data->show_all && is_all_button_text (button_title))
        {
            continue;
        }

        button = gtk_dialog_add_button (GTK_DIALOG (dialog), button_title, response_id);
        gtk_dialog_set_default_response (GTK_DIALOG (dialog), response_id);

        if (g_strcmp0(button_title, DELETE) == 0)
        {
            gtk_style_context_add_class(gtk_widget_get_style_context(button),
                                        "destructive-action");
        }
    }

    if (data->details_text)
    {
        eel_gtk_message_dialog_set_details_label (GTK_MESSAGE_DIALOG (dialog),
                                                  data->details_text);
    }

    /* Run it. */
    result = gtk_dialog_run (GTK_DIALOG (dialog));

    while ((result == GTK_RESPONSE_NONE || result == GTK_RESPONSE_DELETE_EVENT) && data->ignore_close_box)
    {
        result = gtk_dialog_run (GTK_DIALOG (dialog));
    }

    gtk_widget_destroy (dialog);

    data->result = result;
    data->completed = TRUE;

    g_cond_signal (&data->cond);
    g_mutex_unlock (&data->mutex);

    return FALSE;
}

static int
run_simple_dialog_va (NautilusFileTask *task,
                      gboolean          ignore_close_box,
                      GtkMessageType    message_type,
                      char             *primary_text,
                      char             *secondary_text,
                      const char       *details_text,
                      gboolean          show_all,
                      va_list           varargs)
{
    NautilusFileTaskPrivate *priv;
    RunSimpleDialogData *data;
    int res;
    const char *button_title;
    GPtrArray *ptr_array;

    priv = nautilus_file_task_get_instance_private (task);

    data = g_new0 (RunSimpleDialogData, 1);
    data->parent_window = &priv->parent_window;
    data->ignore_close_box = ignore_close_box;
    data->message_type = message_type;
    data->primary_text = primary_text;
    data->secondary_text = secondary_text;
    data->details_text = details_text;
    data->show_all = show_all;
    data->completed = FALSE;
    g_mutex_init (&data->mutex);
    g_cond_init (&data->cond);

    ptr_array = g_ptr_array_new ();
    while ((button_title = va_arg (varargs, const char *)) != NULL)
    {
        g_ptr_array_add (ptr_array, (char *) button_title);
    }
    g_ptr_array_add (ptr_array, NULL);
    data->button_titles = (const char **) g_ptr_array_free (ptr_array, FALSE);

    nautilus_progress_info_pause (priv->progress);

    g_mutex_lock (&data->mutex);

    g_main_context_invoke (NULL,
                           do_run_simple_dialog,
                           data);

    while (!data->completed)
    {
        g_cond_wait (&data->cond, &data->mutex);
    }

    nautilus_progress_info_resume (priv->progress);
    res = data->result;

    g_mutex_unlock (&data->mutex);
    g_mutex_clear (&data->mutex);
    g_cond_clear (&data->cond);

    g_free (data->button_titles);
    g_free (data);

    g_free (primary_text);
    g_free (secondary_text);

    return res;
}

int
nautilus_file_task_prompt_error (NautilusFileTask *self,
                                 char             *primary_text,
                                 char             *secondary_text,
                                 const char       *details_text,
                                 gboolean          show_all,
                                 ...)
{
    va_list varargs;
    int res;

    g_return_val_if_fail (NAUTILUS_IS_FILE_TASK (self), 0);

    va_start (varargs, show_all);
    res = run_simple_dialog_va (self,
                                FALSE,
                                GTK_MESSAGE_ERROR,
                                primary_text,
                                secondary_text,
                                details_text,
                                show_all,
                                varargs);
    va_end (varargs);
    return res;
}

int
nautilus_file_task_prompt_warning (NautilusFileTask  *self,
                                   char              *primary_text,
                                   char              *secondary_text,
                                   const char        *details_text,
                                   gboolean           show_all,
                                   ...)
{
    va_list varargs;
    int res;

    g_return_val_if_fail (NAUTILUS_IS_FILE_TASK (self), 0);

    va_start (varargs, show_all);
    res = run_simple_dialog_va (self,
                                FALSE,
                                GTK_MESSAGE_WARNING,
                                primary_text,
                                secondary_text,
                                details_text,
                                show_all,
                                varargs);
    va_end (varargs);
    return res;
}

static gboolean
has_invalid_xml_char (char *str)
{
    gunichar c;

    while (*str != 0)
    {
        c = g_utf8_get_char (str);
        /* characters XML permits */
        if (!(c == 0x9 ||
              c == 0xA ||
              c == 0xD ||
              (c >= 0x20 && c <= 0xD7FF) ||
              (c >= 0xE000 && c <= 0xFFFD) ||
              (c >= 0x10000 && c <= 0x10FFFF)))
        {
            return TRUE;
        }
        str = g_utf8_next_char (str);
    }
    return FALSE;
}

gchar *
nautilus_file_task_get_basename (GFile *file)
{
    GFileInfo *info;
    gchar *name, *basename, *tmp;
    GMount *mount;

    if ((mount = nautilus_get_mounted_mount_for_root (file)) != NULL)
    {
        name = g_mount_get_name (mount);
        g_object_unref (mount);
    }
    else
    {
        info = g_file_query_info (file,
                                  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                  0,
                                  g_cancellable_get_current (),
                                  NULL);
        name = NULL;
        if (info)
        {
            name = g_strdup (g_file_info_get_display_name (info));
            g_object_unref (info);
        }
    }

    if (name == NULL)
    {
        basename = g_file_get_basename (file);
        if (g_utf8_validate (basename, -1, NULL))
        {
            name = basename;
        }
        else
        {
            name = g_uri_escape_string (basename, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
            g_free (basename);
        }
    }

    /* Some chars can't be put in the markup we use for the dialogs... */
    if (has_invalid_xml_char (name))
    {
        tmp = name;
        name = g_uri_escape_string (name, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
        g_free (tmp);
    }

    /* Finally, if the string is too long, truncate it. */
    if (name != NULL)
    {
        tmp = name;
        name = eel_str_middle_truncate (tmp, MAXIMUM_DISPLAYED_FILE_NAME_LENGTH);
        g_free (tmp);
    }

    return name;
}

gchar *
nautilus_file_task_get_formatted_time (int seconds)
{
    int minutes;
    int hours;
    gchar *res;

    if (seconds < 0)
    {
        /* Just to make sure... */
        seconds = 0;
    }

    if (seconds < 60)
    {
        return g_strdup_printf (ngettext ("%'d second", "%'d seconds", (int) seconds), seconds);
    }

    if (seconds < 60 * 60)
    {
        minutes = seconds / 60;
        return g_strdup_printf (ngettext ("%'d minute", "%'d minutes", minutes), minutes);
    }

    hours = seconds / (60 * 60);

    if (seconds < 60 * 60 * 4)
    {
        gchar *h, *m;

        minutes = (seconds - hours * 60 * 60) / 60;

        h = g_strdup_printf (ngettext ("%'d hour", "%'d hours", hours), hours);
        m = g_strdup_printf (ngettext ("%'d minute", "%'d minutes", minutes), minutes);
        res = g_strconcat (h, ", ", m, NULL);
        g_free (h);
        g_free (m);
        return res;
    }

    return g_strdup_printf (ngettext ("approximately %'d hour",
                                      "approximately %'d hours",
                                      hours), hours);
}

/* keep in time with get_formatted_time ()
 *
 * This counts and outputs the number of “time units”
 * formatted and displayed by get_formatted_time ().
 * For instance, if get_formatted_time outputs “3 hours, 4 minutes”
 * it yields 7.
 */
int
nautilus_file_task_seconds_count_format_time_units (int seconds)
{
    int minutes;
    int hours;

    if (seconds < 0)
    {
        /* Just to make sure... */
        seconds = 0;
    }

    if (seconds < 60)
    {
        /* seconds */
        return seconds;
    }

    if (seconds < 60 * 60)
    {
        /* minutes */
        minutes = seconds / 60;
        return minutes;
    }

    hours = seconds / (60 * 60);

    if (seconds < 60 * 60 * 4)
    {
        /* minutes + hours */
        minutes = (seconds - hours * 60 * 60) / 60;
        return minutes + hours;
    }

    return hours;
}

void
nautilus_file_task_inhibit_power_manager (NautilusFileTask *self,
                                          const char       *message)
{
    NautilusFileTaskPrivate *priv;

    g_return_if_fail (NAUTILUS_IS_FILE_TASK (self));

    priv = nautilus_file_task_get_instance_private (self);

    priv->inhibit_cookie = gtk_application_inhibit (GTK_APPLICATION (g_application_get_default ()),
                                                    GTK_WINDOW (priv->parent_window),
                                                    GTK_APPLICATION_INHIBIT_LOGOUT |
                                                    GTK_APPLICATION_INHIBIT_SUSPEND,
                                                    message);
}
