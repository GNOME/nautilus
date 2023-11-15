/* nautilus-file-name-widget-controller.c
 *
 * Copyright (C) 2016 the Nautilus developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <glib/gi18n.h>

#include "nautilus-file-name-widget-controller.h"
#include "nautilus-file-utilities.h"

#define FILE_NAME_DUPLICATED_LABEL_TIMEOUT 500

typedef struct
{
    GtkWidget *error_revealer;
    GtkWidget *error_label;
    GtkWidget *name_entry;
    GtkWidget *activate_button;
    NautilusDirectory *containing_directory;
    gboolean target_is_folder;
    char *original_name;

    gboolean duplicated_is_folder;
    gint duplicated_label_timeout_id;
} NautilusFileNameWidgetControllerPrivate;

enum
{
    NAME_ACCEPTED,
    CANCELLED,
    LAST_SIGNAL
};

enum
{
    PROP_ERROR_REVEALER = 1,
    PROP_ERROR_LABEL,
    PROP_NAME_ENTRY,
    PROP_ACTION_BUTTON,
    PROP_CONTAINING_DIRECTORY,
    PROP_TARGET_IS_FOLDER,
    NUM_PROPERTIES
};

static guint signals[LAST_SIGNAL];

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (NautilusFileNameWidgetController, nautilus_file_name_widget_controller, G_TYPE_OBJECT)

gchar *
nautilus_file_name_widget_controller_get_new_name (NautilusFileNameWidgetController *self)
{
    return NAUTILUS_FILE_NAME_WIDGET_CONTROLLER_GET_CLASS (self)->get_new_name (self);
}

void
nautilus_file_name_widget_controller_set_target_is_folder (NautilusFileNameWidgetController *self,
                                                           gboolean                          is_folder)
{
    g_object_set (self, "target-is-folder", is_folder, NULL);
}

void
nautilus_file_name_widget_controller_set_original_name (NautilusFileNameWidgetController *self,
                                                        const char                       *original_name)
{
    NautilusFileNameWidgetControllerPrivate *priv = nautilus_file_name_widget_controller_get_instance_private (self);

    g_free (priv->original_name);
    priv->original_name = g_strdup (original_name);
}

void
nautilus_file_name_widget_controller_set_containing_directory (NautilusFileNameWidgetController *self,
                                                               NautilusDirectory                *directory)
{
    g_assert (NAUTILUS_IS_DIRECTORY (directory));

    g_object_set (self, "containing-directory", directory, NULL);
}

static gboolean
nautilus_file_name_widget_controller_is_name_too_long (NautilusFileNameWidgetController *self,
                                                       gchar                            *name)
{
    NautilusFileNameWidgetControllerPrivate *priv;
    size_t name_length;
    g_autoptr (GFile) location = NULL;
    glong max_name_length;

    priv = nautilus_file_name_widget_controller_get_instance_private (self);
    name_length = strlen (name);
    location = nautilus_directory_get_location (priv->containing_directory);
    max_name_length = nautilus_get_max_child_name_length_for_location (location);

    if (max_name_length == -1)
    {
        /* We don't know, so let's give it a chance */
        return FALSE;
    }
    else
    {
        return name_length > (gulong) max_name_length;
    }
}

static gboolean
nautilus_file_name_widget_controller_ignore_existing_file (NautilusFileNameWidgetController *self,
                                                           NautilusFile                     *existing_file)
{
    NautilusFileNameWidgetControllerPrivate *priv = nautilus_file_name_widget_controller_get_instance_private (self);

    return (priv->original_name != NULL &&
            nautilus_file_compare_display_name (existing_file, priv->original_name) == 0);
}

static gchar *
real_get_new_name (NautilusFileNameWidgetController *self)
{
    NautilusFileNameWidgetControllerPrivate *priv;

    priv = nautilus_file_name_widget_controller_get_instance_private (self);

    return g_strstrip (g_strdup (gtk_editable_get_text (GTK_EDITABLE (priv->name_entry))));
}

static gboolean
nautilus_file_name_widget_controller_name_is_valid (NautilusFileNameWidgetController  *self,
                                                    gchar                             *name,
                                                    gchar                            **error_message)
{
    NautilusFileNameWidgetControllerPrivate *priv = nautilus_file_name_widget_controller_get_instance_private (self);
    gboolean is_folder = priv->target_is_folder;
    gboolean is_valid;

    is_valid = TRUE;
    if (strlen (name) == 0)
    {
        is_valid = FALSE;
    }
    else if (strstr (name, "/") != NULL)
    {
        is_valid = FALSE;
        *error_message = is_folder ? _("Folder names cannot contain “/”.") :
                                     _("File names cannot contain “/”.");
    }
    else if (strcmp (name, ".") == 0)
    {
        is_valid = FALSE;
        *error_message = is_folder ? _("A folder cannot be called “.”.") :
                                     _("A file cannot be called “.”.");
    }
    else if (strcmp (name, "..") == 0)
    {
        is_valid = FALSE;
        *error_message = is_folder ? _("A folder cannot be called “..”.") :
                                     _("A file cannot be called “..”.");
    }
    else if (nautilus_file_name_widget_controller_is_name_too_long (self, name))
    {
        is_valid = FALSE;
        *error_message = is_folder ? _("Folder name is too long.") :
                                     _("File name is too long.");
    }

    if (is_valid && g_str_has_prefix (name, "."))
    {
        /* We must warn about the side effect */
        *error_message = is_folder ? _("Folders with “.” at the beginning of their name are hidden.") :
                                     _("Files with “.” at the beginning of their name are hidden.");
    }

    return is_valid;
}

static gboolean
duplicated_file_label_show (NautilusFileNameWidgetController *self)
{
    NautilusFileNameWidgetControllerPrivate *priv;

    priv = nautilus_file_name_widget_controller_get_instance_private (self);
    if (priv->duplicated_is_folder)
    {
        gtk_label_set_label (GTK_LABEL (priv->error_label),
                             _("A folder with that name already exists."));
    }
    else
    {
        gtk_label_set_label (GTK_LABEL (priv->error_label),
                             _("A file with that name already exists."));
    }

    gtk_revealer_set_reveal_child (GTK_REVEALER (priv->error_revealer),
                                   TRUE);

    priv->duplicated_label_timeout_id = 0;

    return G_SOURCE_REMOVE;
}

static void
file_name_widget_controller_process_new_name (NautilusFileNameWidgetController *controller,
                                              gboolean                         *duplicated_name,
                                              gboolean                         *valid_name)
{
    NautilusFileNameWidgetControllerPrivate *priv;
    g_autofree gchar *name = NULL;
    gchar *error_message = NULL;
    NautilusFile *existing_file;
    priv = nautilus_file_name_widget_controller_get_instance_private (controller);

    g_return_if_fail (NAUTILUS_IS_DIRECTORY (priv->containing_directory));

    name = nautilus_file_name_widget_controller_get_new_name (controller);
    *valid_name = nautilus_file_name_widget_controller_name_is_valid (controller,
                                                                      name,
                                                                      &error_message);

    gtk_label_set_label (GTK_LABEL (priv->error_label), error_message);
    gtk_revealer_set_reveal_child (GTK_REVEALER (priv->error_revealer),
                                   error_message != NULL);

    existing_file = nautilus_directory_get_file_by_name (priv->containing_directory, name);
    *duplicated_name = existing_file != NULL &&
                       !nautilus_file_name_widget_controller_ignore_existing_file (controller,
                                                                                   existing_file);

    gtk_widget_set_sensitive (priv->activate_button, *valid_name && !*duplicated_name);

    if (priv->duplicated_label_timeout_id != 0)
    {
        g_source_remove (priv->duplicated_label_timeout_id);
        priv->duplicated_label_timeout_id = 0;
    }

    if (*duplicated_name)
    {
        priv->duplicated_is_folder = nautilus_file_is_directory (existing_file);
    }

    if (existing_file != NULL)
    {
        nautilus_file_unref (existing_file);
    }
}

static void
file_name_widget_controller_on_changed_directory_info_ready (NautilusDirectory *directory,
                                                             GList             *files,
                                                             gpointer           user_data)
{
    NautilusFileNameWidgetController *controller;
    NautilusFileNameWidgetControllerPrivate *priv;
    gboolean duplicated_name;
    gboolean valid_name;

    controller = NAUTILUS_FILE_NAME_WIDGET_CONTROLLER (user_data);
    priv = nautilus_file_name_widget_controller_get_instance_private (controller);

    file_name_widget_controller_process_new_name (controller,
                                                  &duplicated_name,
                                                  &valid_name);

    /* Report duplicated file only if not other message shown (for instance,
     * folders like "." or ".." will always exists, but we consider it as an
     * error, not as a duplicated file or if the name is the same as the file
     * we are renaming also don't report as a duplicated */
    if (duplicated_name && valid_name)
    {
        priv->duplicated_label_timeout_id = g_timeout_add (FILE_NAME_DUPLICATED_LABEL_TIMEOUT,
                                                           (GSourceFunc) duplicated_file_label_show,
                                                           controller);
    }
}

static void
file_name_widget_controller_on_changed (gpointer user_data)
{
    NautilusFileNameWidgetController *controller;
    NautilusFileNameWidgetControllerPrivate *priv;

    controller = NAUTILUS_FILE_NAME_WIDGET_CONTROLLER (user_data);
    priv = nautilus_file_name_widget_controller_get_instance_private (controller);

    nautilus_directory_call_when_ready (priv->containing_directory,
                                        NAUTILUS_FILE_ATTRIBUTE_INFO,
                                        TRUE,
                                        file_name_widget_controller_on_changed_directory_info_ready,
                                        controller);
}

static void
file_name_widget_controller_on_activate_directory_info_ready (NautilusDirectory *directory,
                                                              GList             *files,
                                                              gpointer           user_data)
{
    NautilusFileNameWidgetController *controller;
    gboolean duplicated_name;
    gboolean valid_name;

    controller = NAUTILUS_FILE_NAME_WIDGET_CONTROLLER (user_data);

    file_name_widget_controller_process_new_name (controller,
                                                  &duplicated_name,
                                                  &valid_name);

    if (valid_name && !duplicated_name)
    {
        g_signal_emit (controller, signals[NAME_ACCEPTED], 0);
    }
    else
    {
        /* Report duplicated file only if not other message shown (for instance,
         * folders like "." or ".." will always exists, but we consider it as an
         * error, not as a duplicated file) */
        if (duplicated_name && valid_name)
        {
            /* Show it inmediatily since the user tried to trigger the action */
            duplicated_file_label_show (controller);
        }
    }
}

static void
file_name_widget_controller_on_activate (gpointer user_data)
{
    NautilusFileNameWidgetController *controller;
    NautilusFileNameWidgetControllerPrivate *priv;

    controller = NAUTILUS_FILE_NAME_WIDGET_CONTROLLER (user_data);
    priv = nautilus_file_name_widget_controller_get_instance_private (controller);

    nautilus_directory_call_when_ready (priv->containing_directory,
                                        NAUTILUS_FILE_ATTRIBUTE_INFO,
                                        TRUE,
                                        file_name_widget_controller_on_activate_directory_info_ready,
                                        controller);
}

static void
nautilus_file_name_widget_controller_init (NautilusFileNameWidgetController *self)
{
    NautilusFileNameWidgetControllerPrivate *priv;

    priv = nautilus_file_name_widget_controller_get_instance_private (self);

    priv->containing_directory = NULL;
}

static void
nautilus_file_name_widget_controller_set_property (GObject      *object,
                                                   guint         prop_id,
                                                   const GValue *value,
                                                   GParamSpec   *pspec)
{
    NautilusFileNameWidgetController *controller;
    NautilusFileNameWidgetControllerPrivate *priv;

    controller = NAUTILUS_FILE_NAME_WIDGET_CONTROLLER (object);
    priv = nautilus_file_name_widget_controller_get_instance_private (controller);

    switch (prop_id)
    {
        case PROP_ERROR_REVEALER:
        {
            priv->error_revealer = GTK_WIDGET (g_value_get_object (value));
        }
        break;

        case PROP_ERROR_LABEL:
        {
            priv->error_label = GTK_WIDGET (g_value_get_object (value));
        }
        break;

        case PROP_NAME_ENTRY:
        {
            priv->name_entry = GTK_WIDGET (g_value_get_object (value));

            g_signal_connect_swapped (G_OBJECT (priv->name_entry),
                                      "activate",
                                      (GCallback) file_name_widget_controller_on_activate,
                                      controller);
            g_signal_connect_swapped (G_OBJECT (priv->name_entry),
                                      "changed",
                                      (GCallback) file_name_widget_controller_on_changed,
                                      controller);
        }
        break;

        case PROP_ACTION_BUTTON:
        {
            priv->activate_button = GTK_WIDGET (g_value_get_object (value));

            g_signal_connect_swapped (G_OBJECT (priv->activate_button),
                                      "clicked",
                                      (GCallback) file_name_widget_controller_on_activate,
                                      controller);
        }
        break;

        case PROP_CONTAINING_DIRECTORY:
        {
            g_clear_object (&priv->containing_directory);

            priv->containing_directory = NAUTILUS_DIRECTORY (g_value_dup_object (value));
        }
        break;

        case PROP_TARGET_IS_FOLDER:
        {
            priv->target_is_folder = g_value_get_boolean (value);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
        break;
    }
}

static void
nautilus_file_name_widget_controller_finalize (GObject *object)
{
    NautilusFileNameWidgetController *self;
    NautilusFileNameWidgetControllerPrivate *priv;

    self = NAUTILUS_FILE_NAME_WIDGET_CONTROLLER (object);
    priv = nautilus_file_name_widget_controller_get_instance_private (self);

    if (priv->containing_directory != NULL)
    {
        nautilus_directory_cancel_callback (priv->containing_directory,
                                            file_name_widget_controller_on_changed_directory_info_ready,
                                            self);
        nautilus_directory_cancel_callback (priv->containing_directory,
                                            file_name_widget_controller_on_activate_directory_info_ready,
                                            self);
        g_clear_object (&priv->containing_directory);
    }

    if (priv->duplicated_label_timeout_id > 0)
    {
        g_source_remove (priv->duplicated_label_timeout_id);
        priv->duplicated_label_timeout_id = 0;
    }

    g_free (priv->original_name);

    G_OBJECT_CLASS (nautilus_file_name_widget_controller_parent_class)->finalize (object);
}

static void
nautilus_file_name_widget_controller_class_init (NautilusFileNameWidgetControllerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->set_property = nautilus_file_name_widget_controller_set_property;
    object_class->finalize = nautilus_file_name_widget_controller_finalize;

    klass->get_new_name = real_get_new_name;

    signals[NAME_ACCEPTED] =
        g_signal_new ("name-accepted",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (NautilusFileNameWidgetControllerClass, name_accepted),
                      NULL, NULL,
                      g_cclosure_marshal_generic,
                      G_TYPE_NONE, 0);
    signals[CANCELLED] =
        g_signal_new ("cancelled",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_generic,
                      G_TYPE_NONE, 0);

    g_object_class_install_property (
        object_class,
        PROP_ERROR_REVEALER,
        g_param_spec_object ("error-revealer",
                             "Error Revealer",
                             "The error label revealer",
                             GTK_TYPE_WIDGET,
                             G_PARAM_WRITABLE |
                             G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property (
        object_class,
        PROP_ERROR_LABEL,
        g_param_spec_object ("error-label",
                             "Error Label",
                             "The label used for displaying errors",
                             GTK_TYPE_WIDGET,
                             G_PARAM_WRITABLE |
                             G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (
        object_class,
        PROP_NAME_ENTRY,
        g_param_spec_object ("name-entry",
                             "Name Entry",
                             "The entry for the file name",
                             GTK_TYPE_WIDGET,
                             G_PARAM_WRITABLE |
                             G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (
        object_class,
        PROP_ACTION_BUTTON,
        g_param_spec_object ("activate-button",
                             "Activate Button",
                             "The activate button of the widget",
                             GTK_TYPE_WIDGET,
                             G_PARAM_WRITABLE |
                             G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (
        object_class,
        PROP_CONTAINING_DIRECTORY,
        g_param_spec_object ("containing-directory",
                             "Containing Directory",
                             "The directory used to check for duplicate names",
                             NAUTILUS_TYPE_DIRECTORY,
                             G_PARAM_WRITABLE));
    g_object_class_install_property (
        object_class,
        PROP_TARGET_IS_FOLDER,
        g_param_spec_boolean ("target-is-folder", NULL, NULL,
                              FALSE,
                              G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
}
