/* nautilus-templates-dialog.c
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

#include "nautilus-templates-dialog.h"

#include <glib/gi18n.h>

#include "nautilus-file-operations.h"

struct _NautilusTemplatesDialog
{
    GtkDialog parent_instance;
};

G_DEFINE_TYPE (NautilusTemplatesDialog, nautilus_templates_dialog, GTK_TYPE_DIALOG)

static void
on_import_response (GtkNativeDialog         *native,
                    int                      response,
                    NautilusTemplatesDialog *self)
{
    if (response == GTK_RESPONSE_ACCEPT)
    {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER (native);
        g_autoptr (GFile) file = gtk_file_chooser_get_file (chooser);
        g_autoptr (GFile) templates_location = NULL;
        g_autofree char *path = NULL;

        path = g_file_get_path (file);
        templates_location =
            g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_TEMPLATES));
        nautilus_file_operations_copy_async (&(GList){.data = file},
                                             templates_location,
                                             GTK_WINDOW (self),
                                             NULL, NULL, NULL);
    }

    g_object_unref (native);
}

static void
nautilus_templates_dialog_import_template_dialog (NautilusTemplatesDialog *self G_GNUC_UNUSED)
{
    GtkFileChooserNative *native = gtk_file_chooser_native_new (_("Import Template"),
                                                                GTK_WINDOW (self),
                                                                GTK_FILE_CHOOSER_ACTION_OPEN,
                                                                _("_Import"),
                                                                NULL);

    g_signal_connect (native,
                      "response",
                      G_CALLBACK (on_import_response),
                      self);

    gtk_native_dialog_show (GTK_NATIVE_DIALOG (native));
}

NautilusTemplatesDialog *
nautilus_templates_dialog_new (GtkWindow *parent_window)
{
    return g_object_new (NAUTILUS_TYPE_TEMPLATES_DIALOG,
                         "use-header-bar", TRUE,
                         "transient-for", parent_window, NULL);
}

static void
nautilus_templates_dialog_class_init (NautilusTemplatesDialogClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-templates-dialog.ui");
    gtk_widget_class_bind_template_callback (widget_class, nautilus_templates_dialog_import_template_dialog);
}

static void
nautilus_templates_dialog_init (NautilusTemplatesDialog *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}
