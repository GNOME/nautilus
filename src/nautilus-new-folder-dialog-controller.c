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

#include "nautilus-new-folder-dialog-controller.h"

#include "nautilus-filename-validator.h"

struct _NautilusNewFolderDialogController
{
    AdwWindow parent_instance;

    NautilusFilenameValidator *validator;

    GtkWidget *name_entry;

    gboolean with_selection;
    NewFolderCallback callback;
    gpointer callback_data;
};

G_DEFINE_TYPE (NautilusNewFolderDialogController, nautilus_new_folder_dialog_controller, ADW_TYPE_WINDOW)

static void
on_name_accepted (NautilusNewFolderDialogController *self)
{
    g_autofree char *name = nautilus_filename_validator_get_new_name (self->validator);

    self->callback (name, self->with_selection, self->callback_data);

    gtk_window_close (GTK_WINDOW (self));
}

NautilusNewFolderDialogController *
nautilus_new_folder_dialog_controller_new (GtkWindow         *parent_window,
                                           NautilusDirectory *destination_directory,
                                           gboolean           with_selection,
                                           gchar             *initial_name,
                                           NewFolderCallback  callback,
                                           gpointer           callback_data)
{
    NautilusNewFolderDialogController *self = g_object_new (NAUTILUS_TYPE_NEW_FOLDER_DIALOG_CONTROLLER,
                                                            "transient-for", parent_window,
                                                            NULL);

    nautilus_filename_validator_set_containing_directory (self->validator,
                                                          destination_directory);

    self->with_selection = with_selection;

    self->callback = callback;
    self->callback_data = callback_data;

    if (initial_name != NULL)
    {
        gtk_editable_set_text (GTK_EDITABLE (self->name_entry), initial_name);
    }
    gtk_widget_grab_focus (self->name_entry);

    gtk_window_present (GTK_WINDOW (self));

    return self;
}

static void
nautilus_new_folder_dialog_controller_init (NautilusNewFolderDialogController *self)
{
    g_type_ensure (NAUTILUS_TYPE_FILENAME_VALIDATOR);
    gtk_widget_init_template (GTK_WIDGET (self));
}

static void
nautilus_new_folder_dialog_controller_dispose (GObject *object)
{
    gtk_widget_dispose_template (GTK_WIDGET (object), NAUTILUS_TYPE_NEW_FOLDER_DIALOG_CONTROLLER);

    G_OBJECT_CLASS (nautilus_new_folder_dialog_controller_parent_class)->dispose (object);
}

static void
nautilus_new_folder_dialog_controller_finalize (GObject *object)
{
    G_OBJECT_CLASS (nautilus_new_folder_dialog_controller_parent_class)->finalize (object);
}

static void
nautilus_new_folder_dialog_controller_class_init (NautilusNewFolderDialogControllerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = nautilus_new_folder_dialog_controller_dispose;
    object_class->finalize = nautilus_new_folder_dialog_controller_finalize;

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/nautilus/ui/nautilus-create-folder-dialog.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusNewFolderDialogController, name_entry);
    gtk_widget_class_bind_template_child (widget_class, NautilusNewFolderDialogController, validator);

    gtk_widget_class_bind_template_callback (widget_class, on_name_accepted);
}
