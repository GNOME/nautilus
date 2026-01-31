/* SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2022 Ignacy Kuchciński
 *
 * Authors: Ignacy Kuchciński <ignacykuchcinski@gmail.com>
 */

#include "config.h"

#include "nautilus-file.h"
#include "nautilus-new-file-dialog.h"
#include "nautilus-files-view.h"

#include <glib/gi18n.h>

#include "nautilus-application.h"
#include "nautilus-directory.h"
#include "nautilus-filename-validator.h"

struct _NautilusNewFileDialog
{
    AdwDialog parent_instance;

    NautilusFilenameValidator *validator;

    GtkLabel *extension_label;

    AdwActionRow *text_action_row;
    AdwActionRow *presentation_action_row;

    NewFileCallback callback;
    gpointer *callback_data;

    gchar *template_name;
    gchar *extension;
};

G_DEFINE_FINAL_TYPE (NautilusNewFileDialog, nautilus_new_file_dialog, ADW_TYPE_DIALOG)

static void
new_file_dialog_on_create (AdwDialog             *dialog,
                           NautilusNewFileDialog *self)
{
    g_autofree gchar *name = nautilus_filename_validator_get_new_name (self->validator);
    g_autoptr (NautilusFile) file = NULL;

    if (self->template_name != NULL)
    {
        g_autofree gchar *template_path = g_build_filename (NAUTILUS_DATADIR, "templates",
                                                            self->template_name, NULL);
        g_autoptr (GFile) template_location = g_file_new_for_path (template_path);

        file = nautilus_file_get (template_location);
    }

    self->callback (name, file, self->callback_data);

    adw_dialog_close (ADW_DIALOG (self));
}

static void
text_action_row_on_activated (GtkCheckButton        *check_button,
                              NautilusNewFileDialog *self)
{
    self->template_name = NULL;
    self->extension = ".txt";
    gtk_label_set_text (self->extension_label, ".txt");
}

static void
document_action_row_on_activated (GtkCheckButton        *check_button,
                                  NautilusNewFileDialog *self)
{
    self->template_name = "document.odt";
    self->extension = ".odt";
    gtk_label_set_text (self->extension_label, ".odt");
}

static void
spreadsheet_action_row_on_activated (GtkCheckButton        *check_button,
                                     NautilusNewFileDialog *self)
{
    self->template_name = "spreadsheet.ods";
    self->extension = ".ods";
    gtk_label_set_text (self->extension_label, ".ods");
}

static void
presentation_action_row_on_activated (GtkCheckButton        *check_button,
                                      NautilusNewFileDialog *self)
{
    self->template_name = "presentation.odp";
    self->extension = ".odp";
    gtk_label_set_text (self->extension_label, ".odp");
}

static void
nautilus_new_file_dialog_init (NautilusNewFileDialog *self)
{
    g_type_ensure (NAUTILUS_TYPE_FILENAME_VALIDATOR);
    gtk_widget_init_template (GTK_WIDGET (self));
}

static void
nautilus_new_file_dialog_dispose (GObject *object)
{
    gtk_widget_dispose_template (GTK_WIDGET (object), NAUTILUS_TYPE_NEW_FILE_DIALOG);

    G_OBJECT_CLASS (nautilus_new_file_dialog_parent_class)->dispose (object);
}

static void
nautilus_new_file_dialog_class_init (NautilusNewFileDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = nautilus_new_file_dialog_dispose;

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/nautilus/ui/nautilus-new-file-dialog.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusNewFileDialog, validator);
    gtk_widget_class_bind_template_child (widget_class, NautilusNewFileDialog, presentation_action_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusNewFileDialog, text_action_row);
    gtk_widget_class_bind_template_child (widget_class, NautilusNewFileDialog, extension_label);

    gtk_widget_class_bind_template_callback (widget_class, nautilus_filename_validator_try_accept);
    gtk_widget_class_bind_template_callback (widget_class, nautilus_filename_validator_validate);
    gtk_widget_class_bind_template_callback (widget_class, new_file_dialog_on_create);
    gtk_widget_class_bind_template_callback (widget_class, text_action_row_on_activated);
    gtk_widget_class_bind_template_callback (widget_class, document_action_row_on_activated);
    gtk_widget_class_bind_template_callback (widget_class, spreadsheet_action_row_on_activated);
    gtk_widget_class_bind_template_callback (widget_class, presentation_action_row_on_activated);
}

void
nautilus_new_file_dialog_new (GtkWidget         *parent_window,
                              NautilusDirectory *destination_directory,
                              NewFileCallback    callback,
                              gpointer           callback_data)
{
    NautilusNewFileDialog *self = g_object_new (NAUTILUS_TYPE_NEW_FILE_DIALOG,
                                                NULL);
    self->callback = callback;
    self->callback_data = callback_data;

    nautilus_filename_validator_set_containing_directory (self->validator,
                                                          destination_directory);

    if (!nautilus_application_is_sandboxed ())
    {
        GtkWidget *action_row = GTK_WIDGET (self->presentation_action_row);
        GList *recommended_apps =
            g_app_info_get_recommended_for_type ("application/vnd.oasis.opendocument.presentation");

        gtk_widget_set_visible (action_row, recommended_apps != NULL);
        g_list_free_full (recommended_apps, g_object_unref);

        recommended_apps =
            g_app_info_get_recommended_for_type ("application/vnd.oasis.opendocument.spreadsheet");

        gtk_widget_set_visible (action_row, recommended_apps != NULL);
        g_list_free_full (recommended_apps, g_object_unref);

        recommended_apps =
            g_app_info_get_recommended_for_type ("application/vnd.oasis.opendocument.text");

        gtk_widget_set_visible (action_row, recommended_apps != NULL);
        g_list_free_full (recommended_apps, g_object_unref);
    }

    adw_action_row_activate (self->text_action_row);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent_window));
}
