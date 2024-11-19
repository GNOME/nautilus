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
    AdwDialog parent_instance;
};

G_DEFINE_TYPE (NautilusTemplatesDialog, nautilus_templates_dialog, ADW_TYPE_DIALOG)

static void
on_import_response (GtkFileDialog           *chooser_dialog,
                    GAsyncResult            *result,
                    NautilusTemplatesDialog *self)
{
    g_autoptr (GError) error = NULL;
    g_autoptr (GFile) file = NULL;

    file = gtk_file_dialog_open_finish (chooser_dialog, result, &error);

    if (file != NULL)
    {
        g_autofree char *path = g_file_get_path (file);
        g_autoptr (GFile) templates_location =
            g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_TEMPLATES));

        nautilus_file_operations_copy_async (&(GList){.data = file},
                                             templates_location,
                                             GTK_WINDOW (self),
                                             NULL, NULL, NULL);
    }
}

static void
nautilus_templates_dialog_import_template_dialog (NautilusTemplatesDialog *self G_GNUC_UNUSED)
{
    GtkFileDialog *chooser_dialog = gtk_file_dialog_new ();

    gtk_file_dialog_set_accept_label (chooser_dialog, _("_Import"));
    gtk_file_dialog_set_title (chooser_dialog, _("Import Template"));

    gtk_file_dialog_open (chooser_dialog, GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))),
                          NULL, (GAsyncReadyCallback) on_import_response, self);
}

void
nautilus_templates_dialog_new (GtkWindow *parent_window)
{
    NautilusTemplatesDialog *self = g_object_new (NAUTILUS_TYPE_TEMPLATES_DIALOG,
                                                  NULL);

    adw_dialog_present (ADW_DIALOG (self), GTK_WIDGET (parent_window));
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
