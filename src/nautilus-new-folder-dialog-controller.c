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
#include <adwaita.h>

#include "nautilus-new-folder-dialog-controller.h"

#include "nautilus-filename-message.h"


struct _NautilusNewFolderDialogController
{
    AdwWindow parent_instance;

    GtkEditable *name_entry;
    GtkWidget *activate_button;
    GtkRevealer *error_revealer;
    GtkLabel *error_label;

    gboolean from_selection;
    NautilusDirectory *containing_directory;

    NewFolderCallback callback;
    gpointer callback_data;

    gboolean should_create;
};

G_DEFINE_TYPE (NautilusNewFolderDialogController, nautilus_new_folder_dialog_controller, ADW_TYPE_WINDOW)

static void
update_state (NautilusDirectory *,
              GList *,
              gpointer user_data)
{
    NautilusNewFolderDialogController *self = NAUTILUS_NEW_FOLDER_DIALOG_CONTROLLER (user_data);
    g_autofree char *name = g_strstrip (g_strdup (gtk_editable_get_text (self->name_entry)));
    NautilusFileNameMessage message = nautilus_filename_message_from_name (name,
                                                                           self->containing_directory,
                                                                           NULL);
    gboolean is_valid = nautilus_filename_message_is_valid (message);
    const char *error_message = nautilus_filename_message_folder_error (message);

    gtk_label_set_label (GTK_LABEL (self->error_label), error_message);
    gtk_revealer_set_reveal_child (self->error_revealer, error_message != NULL);
    gtk_widget_set_sensitive (self->activate_button, is_valid);

    if (self->should_create && is_valid)
    {
        self->callback (name, self->from_selection, self->callback_data);
    }
    else
    {
        self->should_create = FALSE;
    }
}

static void
on_name_changed (NautilusNewFolderDialogController *self)
{
    nautilus_directory_call_when_ready (self->containing_directory,
                                        NAUTILUS_FILE_ATTRIBUTE_INFO,
                                        TRUE,
                                        update_state,
                                        self);
}

static void
try_accept_name (NautilusNewFolderDialogController *self)
{
    self->should_create = TRUE;
    on_name_changed (self);
}

NautilusNewFolderDialogController *
nautilus_new_folder_dialog_controller_new (NautilusDirectory *destination_directory,
                                           gboolean           from_selection,
                                           gchar             *initial_name,
                                           NewFolderCallback  callback,
                                           gpointer           callback_data)
{
    NautilusNewFolderDialogController *self = g_object_new (NAUTILUS_TYPE_NEW_FOLDER_DIALOG_CONTROLLER, NULL);

    self->from_selection = from_selection;
    self->containing_directory = nautilus_directory_ref (destination_directory);
    self->callback = callback;
    self->callback_data = callback_data;

    if (initial_name != NULL)
    {
        gtk_editable_set_text (GTK_EDITABLE (self->name_entry), initial_name);
    }
    gtk_widget_grab_focus (GTK_WIDGET (self->name_entry));

    return self;
}

static void
nautilus_new_folder_dialog_controller_init (NautilusNewFolderDialogController *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}

static void
new_folder_dialog_dispose (GObject *object)
{
    gtk_widget_dispose_template (GTK_WIDGET (object), NAUTILUS_TYPE_NEW_FOLDER_DIALOG_CONTROLLER);

    G_OBJECT_CLASS (nautilus_new_folder_dialog_controller_parent_class)->dispose (object);
}


static void
nautilus_new_folder_dialog_controller_finalize (GObject *object)
{
    NautilusNewFolderDialogController *self;

    self = NAUTILUS_NEW_FOLDER_DIALOG_CONTROLLER (object);

    g_clear_object (&self->containing_directory);

    G_OBJECT_CLASS (nautilus_new_folder_dialog_controller_parent_class)->finalize (object);
}

static void
nautilus_new_folder_dialog_controller_class_init (NautilusNewFolderDialogControllerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = nautilus_new_folder_dialog_controller_finalize;
    object_class->dispose = new_folder_dialog_dispose;

    gtk_widget_class_set_template_from_resource (
        widget_class, "/org/gnome/nautilus/ui/nautilus-create-folder-dialog.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusNewFolderDialogController, activate_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusNewFolderDialogController, error_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusNewFolderDialogController, error_revealer);
    gtk_widget_class_bind_template_child (widget_class, NautilusNewFolderDialogController, name_entry);

    gtk_widget_class_bind_template_callback (widget_class, on_name_changed);
    gtk_widget_class_bind_template_callback (widget_class, try_accept_name);
}
