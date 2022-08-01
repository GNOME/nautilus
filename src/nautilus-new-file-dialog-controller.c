/* nautilus-new-file-dialog-controller.c
 *
 * Copyright 2022 Ignacy Kuchciński <ignacykuchcinski@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-new-file-dialog-controller.h"

#include <glib/gi18n.h>

#include "nautilus-application.h"

struct _NautilusNewFileDialogController
{
    NautilusFileNameWidgetController parent_instance;

    GtkDialog *new_file_dialog;
    GtkLabel *extension_label;

    gchar *template_name;
    gchar *extension;
};

G_DEFINE_TYPE (NautilusNewFileDialogController, nautilus_new_file_dialog_controller, NAUTILUS_TYPE_FILE_NAME_WIDGET_CONTROLLER)

static gboolean
nautilus_new_file_dialog_controller_name_is_valid (NautilusFileNameWidgetController  *self,
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
        *error_message = _("File names cannot contain “/“.");
    }
    else if (strcmp (name, ".") == 0)
    {
        is_valid = FALSE;
        *error_message = _("A file cannot be called “.“.");
    }
    else if (strcmp (name, "..") == 0)
    {
        is_valid = FALSE;
        *error_message = _("A file cannot be called “..“.");
    }
    else if (nautilus_file_name_widget_controller_is_name_too_long (self, name))
    {
        is_valid = FALSE;
        *error_message = _("File name is too long.");
    }
    else if (g_str_has_prefix (name, "."))
    {
        /* We must warn about the side effect */
        *error_message = _("Files with “.“ at the beginning of their name are hidden.");
    }

    return is_valid;
}

static gchar *
nautilus_new_file_dialog_controller_get_new_name (NautilusFileNameWidgetController *controller)
{
    NautilusNewFileDialogController *self;
    g_autofree gchar *basename = NULL;

    self = NAUTILUS_NEW_FILE_DIALOG_CONTROLLER (controller);

    basename = NAUTILUS_FILE_NAME_WIDGET_CONTROLLER_CLASS (nautilus_new_file_dialog_controller_parent_class)->get_new_name (controller);

    if (g_str_has_suffix (basename, self->extension))
    {
        return g_strdup (basename);
    }

    return g_strconcat (basename, self->extension, NULL);
}


gchar *
nautilus_new_file_dialog_controller_get_template_name (NautilusNewFileDialogController *self)
{
    return self->template_name;
}

static void
new_file_dialog_controller_on_response (GtkDialog                       *dialog,
                                        int                              response,
                                        NautilusNewFileDialogController *self)
{
    if (response != GTK_RESPONSE_OK)
    {
        g_signal_emit_by_name (self, "cancelled");
    }
}

static void
text_action_row_on_activated (GtkCheckButton                  *check_button,
                              NautilusNewFileDialogController *self)
{
    self->template_name = NULL;
    self->extension = ".txt";
    gtk_label_set_text (self->extension_label, ".txt");
}

static void
document_action_row_on_activated (GtkCheckButton                  *check_button,
                                  NautilusNewFileDialogController *self)
{
    self->template_name = "document.odt";
    self->extension = ".odt";
    gtk_label_set_text (self->extension_label, ".odt");
}

static void
spreadsheet_action_row_on_activated (GtkCheckButton                  *check_button,
                                     NautilusNewFileDialogController *self)
{
    self->template_name = "spreadsheet.ods";
    self->extension = ".ods";
    gtk_label_set_text (self->extension_label, ".ods");
}

static void
presentation_action_row_on_activated (GtkCheckButton                  *check_button,
                                      NautilusNewFileDialogController *self)
{
    self->template_name = "presentation.odp";
    self->extension = ".odp";
    gtk_label_set_text (self->extension_label, ".odp");
}

NautilusNewFileDialogController *
nautilus_new_file_dialog_controller_new (GtkWindow         *parent_window,
                                         NautilusDirectory *destination_directory)
{
    NautilusNewFileDialogController *self;
    g_autoptr (GtkBuilder) builder = NULL;
    GtkDialog *new_file_dialog;
    GtkLabel *extension_label;
    GtkRevealer *error_revealer;
    GtkLabel *error_label;
    AdwEntryRow *name_entry;
    GtkButton *activate_button;
    GList *recommended_apps;
    AdwActionRow *text_action_row;
    AdwActionRow *document_action_row;
    AdwActionRow *spreadsheet_action_row;
    AdwActionRow *presentation_action_row;
    gboolean not_sandboxed;

    builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-create-file-dialog.ui");
    new_file_dialog = GTK_DIALOG (gtk_builder_get_object (builder, "create_file_dialog"));
    extension_label = GTK_LABEL (gtk_builder_get_object (builder, "extension_label"));
    error_revealer = GTK_REVEALER (gtk_builder_get_object (builder, "error_revealer"));
    error_label = GTK_LABEL (gtk_builder_get_object (builder, "error_label"));
    name_entry = ADW_ENTRY_ROW (gtk_builder_get_object (builder, "name_entry"));
    activate_button = GTK_BUTTON (gtk_builder_get_object (builder, "ok_button"));
    text_action_row = ADW_ACTION_ROW (gtk_builder_get_object (builder, "text_action_row"));
    document_action_row = ADW_ACTION_ROW (gtk_builder_get_object (builder, "document_action_row"));
    spreadsheet_action_row = ADW_ACTION_ROW (gtk_builder_get_object (builder, "spreadsheet_action_row"));
    presentation_action_row = ADW_ACTION_ROW (gtk_builder_get_object (builder, "presentation_action_row"));

    gtk_window_set_transient_for (GTK_WINDOW (new_file_dialog), parent_window);

    self = g_object_new (NAUTILUS_TYPE_NEW_FILE_DIALOG_CONTROLLER,
                         "error-revealer", error_revealer,
                         "error-label", error_label,
                         "name-entry", name_entry,
                         "activate-button", activate_button,
                         "containing-directory", destination_directory, NULL);

    self->new_file_dialog = new_file_dialog;
    self->extension_label = extension_label;

    g_signal_connect_object (new_file_dialog,
                             "response",
                             G_CALLBACK (new_file_dialog_controller_on_response),
                             self,
                             0);

    g_signal_connect_object (text_action_row,
                             "activated",
                             G_CALLBACK (text_action_row_on_activated),
                             self,
                             0);

    g_signal_connect_object (document_action_row,
                             "activated",
                             G_CALLBACK (document_action_row_on_activated),
                             self,
                             0);

    g_signal_connect_object (spreadsheet_action_row,
                             "activated",
                             G_CALLBACK (spreadsheet_action_row_on_activated),
                             self,
                             0);

    g_signal_connect_object (presentation_action_row,
                             "activated",
                             G_CALLBACK (presentation_action_row_on_activated),
                             self,
                             0);

    recommended_apps = g_app_info_get_recommended_for_type ("application/vnd.oasis.opendocument.presentation");
    not_sandboxed = !nautilus_application_is_sandboxed ();

    if (recommended_apps == NULL && not_sandboxed)
    {
        gtk_widget_set_visible (GTK_WIDGET (presentation_action_row), FALSE);
    }
    else
    {
        g_list_free_full (recommended_apps, g_object_unref);
    }

    recommended_apps = g_app_info_get_recommended_for_type ("application/vnd.oasis.opendocument.spreadsheet");

    if (recommended_apps == NULL && not_sandboxed)
    {
        gtk_widget_set_visible (GTK_WIDGET (spreadsheet_action_row), FALSE);
    }
    else
    {
        g_list_free_full (recommended_apps, g_object_unref);
    }

    recommended_apps = g_app_info_get_recommended_for_type ("application/vnd.oasis.opendocument.text");

    if (recommended_apps == NULL && not_sandboxed)
    {
        gtk_widget_set_visible (GTK_WIDGET (document_action_row), FALSE);
    }
    else
    {
        g_list_free_full (recommended_apps, g_object_unref);
    }

    adw_action_row_activate (text_action_row);

    gtk_widget_show (GTK_WIDGET (new_file_dialog));

    return self;
}

static void
nautilus_new_file_dialog_controller_init (NautilusNewFileDialogController *self)
{
}

static void
nautilus_new_file_dialog_controller_finalize (GObject *gobject)
{
    NautilusNewFileDialogController *self;

    self = NAUTILUS_NEW_FILE_DIALOG_CONTROLLER (gobject);

    if (self->new_file_dialog != NULL)
    {
        gtk_window_destroy (GTK_WINDOW (self->new_file_dialog));
        self->new_file_dialog = NULL;
    }

    G_OBJECT_CLASS (nautilus_new_file_dialog_controller_parent_class)->finalize (gobject);
}

static void
nautilus_new_file_dialog_controller_class_init (NautilusNewFileDialogControllerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    NautilusFileNameWidgetControllerClass *parent_class = NAUTILUS_FILE_NAME_WIDGET_CONTROLLER_CLASS (klass);

    object_class->finalize = nautilus_new_file_dialog_controller_finalize;

    parent_class->name_is_valid = nautilus_new_file_dialog_controller_name_is_valid;
    parent_class->get_new_name = nautilus_new_file_dialog_controller_get_new_name;
}
