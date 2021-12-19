/* nautilus-new-folder-dialog-controller.c
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

#include <eel/eel-vfs-extensions.h>

#include "nautilus-new-folder-dialog-controller.h"


struct _NautilusNewFolderDialogController
{
    NautilusFileNameWidgetController parent_instance;

    GtkWidget *new_folder_dialog;

    gboolean with_selection;

    gulong response_handler_id;
};

G_DEFINE_TYPE (NautilusNewFolderDialogController, nautilus_new_folder_dialog_controller, NAUTILUS_TYPE_FILE_NAME_WIDGET_CONTROLLER)

static gboolean
nautilus_new_folder_dialog_controller_name_is_valid (NautilusFileNameWidgetController  *self,
                                                     gchar                             *name,
                                                     gchar                            **error_message)
{
    gboolean is_valid;

    is_valid = TRUE;
    if (strlen (name) == 0)
    {
        is_valid = FALSE;
    }
    else if (strstr (name, "/") != NULL)
    {
        is_valid = FALSE;
        *error_message = _("Folder names cannot contain “/”.");
    }
    else if (strcmp (name, ".") == 0)
    {
        is_valid = FALSE;
        *error_message = _("A folder cannot be called “.”.");
    }
    else if (strcmp (name, "..") == 0)
    {
        is_valid = FALSE;
        *error_message = _("A folder cannot be called “..”.");
    }
    else if (nautilus_file_name_widget_controller_is_name_too_long (self, name))
    {
        is_valid = FALSE;
        *error_message = _("Folder name is too long.");
    }

    if (is_valid && g_str_has_prefix (name, "."))
    {
        /* We must warn about the side effect */
        *error_message = _("Folders with “.” at the beginning of their name are hidden.");
        return TRUE;
    }

    return is_valid;
}

static void
new_folder_dialog_controller_on_response (GtkDialog *dialog,
                                          gint       response_id,
                                          gpointer   user_data)
{
    NautilusNewFolderDialogController *controller;

    controller = NAUTILUS_NEW_FOLDER_DIALOG_CONTROLLER (user_data);

    if (response_id != GTK_RESPONSE_OK)
    {
        g_signal_emit_by_name (controller, "cancelled");
    }
}

NautilusNewFolderDialogController *
nautilus_new_folder_dialog_controller_new (GtkWindow         *parent_window,
                                           NautilusDirectory *destination_directory,
                                           gboolean           with_selection,
                                           gchar             *initial_name)
{
    NautilusNewFolderDialogController *self;
    g_autoptr (GtkBuilder) builder = NULL;
    GtkWidget *new_folder_dialog;
    GtkWidget *error_revealer;
    GtkWidget *error_label;
    GtkWidget *name_entry;
    GtkWidget *activate_button;
    GtkWidget *name_label;

    builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-create-folder-dialog.ui");
    new_folder_dialog = GTK_WIDGET (gtk_builder_get_object (builder, "create_folder_dialog"));
    error_revealer = GTK_WIDGET (gtk_builder_get_object (builder, "error_revealer"));
    error_label = GTK_WIDGET (gtk_builder_get_object (builder, "error_label"));
    name_entry = GTK_WIDGET (gtk_builder_get_object (builder, "name_entry"));
    activate_button = GTK_WIDGET (gtk_builder_get_object (builder, "ok_button"));
    name_label = GTK_WIDGET (gtk_builder_get_object (builder, "name_label"));

    gtk_window_set_transient_for (GTK_WINDOW (new_folder_dialog),
                                  parent_window);

    self = g_object_new (NAUTILUS_TYPE_NEW_FOLDER_DIALOG_CONTROLLER,
                         "error-revealer", error_revealer,
                         "error-label", error_label,
                         "name-entry", name_entry,
                         "activate-button", activate_button,
                         "containing-directory", destination_directory, NULL);

    self->with_selection = with_selection;

    self->new_folder_dialog = new_folder_dialog;

    self->response_handler_id = g_signal_connect (new_folder_dialog,
                                                  "response",
                                                  (GCallback) new_folder_dialog_controller_on_response,
                                                  self);

    if (initial_name != NULL)
    {
        gtk_editable_set_text (GTK_EDITABLE (name_entry), initial_name);
    }

    gtk_button_set_label (GTK_BUTTON (activate_button), _("Create"));
    gtk_label_set_text (GTK_LABEL (name_label), _("Folder name"));
    gtk_window_set_title (GTK_WINDOW (new_folder_dialog), _("New Folder"));

    gtk_widget_show_all (new_folder_dialog);

    return self;
}

gboolean
nautilus_new_folder_dialog_controller_get_with_selection (NautilusNewFolderDialogController *self)
{
    return self->with_selection;
}

static void
nautilus_new_folder_dialog_controller_init (NautilusNewFolderDialogController *self)
{
}

static void
nautilus_new_folder_dialog_controller_finalize (GObject *object)
{
    NautilusNewFolderDialogController *self;

    self = NAUTILUS_NEW_FOLDER_DIALOG_CONTROLLER (object);

    if (self->new_folder_dialog != NULL)
    {
        g_clear_signal_handler (&self->response_handler_id, self->new_folder_dialog);
        gtk_window_destroy (GTK_WINDOW (self->new_folder_dialog));
        self->new_folder_dialog = NULL;
    }

    G_OBJECT_CLASS (nautilus_new_folder_dialog_controller_parent_class)->finalize (object);
}

static void
nautilus_new_folder_dialog_controller_class_init (NautilusNewFolderDialogControllerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    NautilusFileNameWidgetControllerClass *parent_class = NAUTILUS_FILE_NAME_WIDGET_CONTROLLER_CLASS (klass);

    object_class->finalize = nautilus_new_folder_dialog_controller_finalize;

    parent_class->name_is_valid = nautilus_new_folder_dialog_controller_name_is_valid;
}
