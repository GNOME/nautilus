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

typedef struct
{
    GtkWidget *error_revealer;
    GtkWidget *error_label;
    GtkWidget *name_entry;
    GtkWidget *activate_button;
    NautilusDirectory *containing_directory;

    gboolean duplicated_is_folder;
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
    NUM_PROPERTIES
};

static guint signals[LAST_SIGNAL];

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (NautilusFileNameWidgetController, nautilus_file_name_widget_controller, G_TYPE_OBJECT)

void
nautilus_file_name_widget_controller_set_containing_directory (NautilusFileNameWidgetController *self,
                                                               NautilusDirectory                *directory)
{
    g_assert (NAUTILUS_IS_DIRECTORY (directory));

    g_object_set (self, "containing-directory", directory, NULL);
}

gboolean
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
        return name_length > max_name_length;
    }
}

static void
file_name_widget_controller_on_changed_directory_info_ready (NautilusDirectory *directory,
                                                             GList             *files,
                                                             gpointer           user_data)
{
    NAUTILUS_FILE_NAME_WIDGET_CONTROLLER_GET_CLASS (user_data)->update_name (user_data);
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
    gboolean valid_name;

    valid_name = NAUTILUS_FILE_NAME_WIDGET_CONTROLLER_GET_CLASS (user_data)->update_name (user_data);

    if (valid_name)
    {
        NautilusFileNameWidgetController *controller = NAUTILUS_FILE_NAME_WIDGET_CONTROLLER (user_data);
        g_signal_emit (controller, signals[NAME_ACCEPTED], 0);
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

    G_OBJECT_CLASS (nautilus_file_name_widget_controller_parent_class)->finalize (object);
}

static void
nautilus_file_name_widget_controller_class_init (NautilusFileNameWidgetControllerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->set_property = nautilus_file_name_widget_controller_set_property;
    object_class->finalize = nautilus_file_name_widget_controller_finalize;

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
}
