/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>

#include "nautilus-new-folder-dialog.h"

#include "nautilus-filename-validator.h"

struct _NautilusNewFolderDialog
{
    AdwWindow parent_instance;

    NautilusFilenameValidator *validator;

    GtkWidget *name_entry;

    gboolean with_selection;
    NewFolderCallback callback;
    gpointer callback_data;
};

G_DEFINE_TYPE (NautilusNewFolderDialog, nautilus_new_folder_dialog, ADW_TYPE_WINDOW)

static void
on_name_accepted (NautilusNewFolderDialog *self)
{
    g_autofree char *name = nautilus_filename_validator_get_new_name (self->validator);

    self->callback (name, self->with_selection, self->callback_data);

    gtk_window_close (GTK_WINDOW (self));
}

NautilusNewFolderDialog *
nautilus_new_folder_dialog_new (GtkWindow         *parent_window,
                                NautilusDirectory *destination_directory,
                                gboolean           with_selection,
                                gchar             *initial_name,
                                NewFolderCallback  callback,
                                gpointer           callback_data)
{
    NautilusNewFolderDialog *self = g_object_new (NAUTILUS_TYPE_NEW_FOLDER_DIALOG,
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
nautilus_new_folder_dialog_init (NautilusNewFolderDialog *self)
{
    g_type_ensure (NAUTILUS_TYPE_FILENAME_VALIDATOR);
    gtk_widget_init_template (GTK_WIDGET (self));
}

static void
nautilus_new_folder_dialog_dispose (GObject *object)
{
    gtk_widget_dispose_template (GTK_WIDGET (object), NAUTILUS_TYPE_NEW_FOLDER_DIALOG);

    G_OBJECT_CLASS (nautilus_new_folder_dialog_parent_class)->dispose (object);
}

static void
nautilus_new_folder_dialog_finalize (GObject *object)
{
    G_OBJECT_CLASS (nautilus_new_folder_dialog_parent_class)->finalize (object);
}

static void
nautilus_new_folder_dialog_class_init (NautilusNewFolderDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = nautilus_new_folder_dialog_dispose;
    object_class->finalize = nautilus_new_folder_dialog_finalize;

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/nautilus/ui/nautilus-new-folder-dialog.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusNewFolderDialog, name_entry);
    gtk_widget_class_bind_template_child (widget_class, NautilusNewFolderDialog, validator);

    gtk_widget_class_bind_template_callback (widget_class, on_name_accepted);
    gtk_widget_class_bind_template_callback (widget_class, nautilus_filename_validator_try_accept);
    gtk_widget_class_bind_template_callback (widget_class, nautilus_filename_validator_validate);
}
