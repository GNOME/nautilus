/* nautilus-compress-dialog-controller.h
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
#include <gnome-autoar/gnome-autoar.h>

#include <eel/eel-vfs-extensions.h>

#include "nautilus-compress-dialog-controller.h"

#include "nautilus-global-preferences.h"

struct _NautilusCompressDialogController
{
    NautilusFileNameWidgetController parent_instance;

    GtkWidget *compress_dialog;
    GtkWidget *description_stack;
    GtkWidget *name_entry;
    GtkWidget *zip_radio_button;
    GtkWidget *tar_xz_radio_button;
    GtkWidget *seven_zip_radio_button;

    const char *extension;

    gint response_handler_id;
};

G_DEFINE_TYPE (NautilusCompressDialogController, nautilus_compress_dialog_controller, NAUTILUS_TYPE_FILE_NAME_WIDGET_CONTROLLER);

static gboolean
nautilus_compress_dialog_controller_name_is_valid (NautilusFileNameWidgetController  *self,
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
        *error_message = _("Archive names cannot contain “/”.");
    }
    else if (strcmp (name, ".") == 0)
    {
        is_valid = FALSE;
        *error_message = _("An archive cannot be called “.”.");
    }
    else if (strcmp (name, "..") == 0)
    {
        is_valid = FALSE;
        *error_message = _("An archive cannot be called “..”.");
    }
    else if (nautilus_file_name_widget_controller_is_name_too_long (self, name))
    {
        is_valid = FALSE;
        *error_message = _("Archive name is too long.");
    }

    if (is_valid && g_str_has_prefix (name, "."))
    {
        /* We must warn about the side effect */
        *error_message = _("Archives with “.” at the beginning of their name are hidden.");
    }

    return is_valid;
}

static gchar *
nautilus_compress_dialog_controller_get_new_name (NautilusFileNameWidgetController *controller)
{
    NautilusCompressDialogController *self;
    g_autofree gchar *basename = NULL;
    gchar *error_message = NULL;
    gboolean valid_name;

    self = NAUTILUS_COMPRESS_DIALOG_CONTROLLER (controller);

    basename = NAUTILUS_FILE_NAME_WIDGET_CONTROLLER_CLASS (nautilus_compress_dialog_controller_parent_class)->get_new_name (controller);
    /* Do not check or add the extension if the name is invalid */
    valid_name = nautilus_compress_dialog_controller_name_is_valid (controller,
                                                                    basename,
                                                                    &error_message);

    if (!valid_name)
    {
        return g_strdup (basename);
    }

    if (g_str_has_suffix (basename, self->extension))
    {
        return g_strdup (basename);
    }

    return g_strconcat (basename, self->extension, NULL);
}

static void
compress_dialog_controller_on_response (GtkDialog *dialog,
                                        gint       response_id,
                                        gpointer   user_data)
{
    NautilusCompressDialogController *controller;

    controller = NAUTILUS_COMPRESS_DIALOG_CONTROLLER (user_data);

    if (response_id != GTK_RESPONSE_OK)
    {
        g_signal_emit_by_name (controller, "cancelled");
    }
}

static void
update_selected_format (NautilusCompressDialogController *self,
                        NautilusCompressionFormat         format)
{
    const char *extension;
    const char *description_label_name;
    GtkWidget *active_button;

    switch (format)
    {
        case NAUTILUS_COMPRESSION_ZIP:
        {
            extension = ".zip";
            description_label_name = "zip-description-label";
            active_button = self->zip_radio_button;
        }
        break;

        case NAUTILUS_COMPRESSION_TAR_XZ:
        {
            extension = ".tar.xz";
            description_label_name = "tar-xz-description-label";
            active_button = self->tar_xz_radio_button;
        }
        break;

        case NAUTILUS_COMPRESSION_7ZIP:
        {
            extension = ".7z";
            description_label_name = "seven-zip-description-label";
            active_button = self->seven_zip_radio_button;
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
        break;
    }

    self->extension = extension;

    gtk_stack_set_visible_child_name (GTK_STACK (self->description_stack),
                                      description_label_name);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (active_button),
                                  TRUE);

    g_settings_set_enum (nautilus_compression_preferences,
                         NAUTILUS_PREFERENCES_DEFAULT_COMPRESSION_FORMAT,
                         format);
    /* Since the extension changes when the button is toggled, force a
     * verification of the new file name by simulating an entry change
     */
    g_signal_emit_by_name (self->name_entry, "changed");
}

static void
zip_radio_button_on_toggled (GtkToggleButton *toggle_button,
                             gpointer         user_data)
{
    NautilusCompressDialogController *controller;

    controller = NAUTILUS_COMPRESS_DIALOG_CONTROLLER (user_data);

    if (!gtk_toggle_button_get_active (toggle_button))
    {
        return;
    }

    update_selected_format (controller,
                            NAUTILUS_COMPRESSION_ZIP);
}

static void
tar_xz_radio_button_on_toggled (GtkToggleButton *toggle_button,
                                gpointer         user_data)
{
    NautilusCompressDialogController *controller;

    controller = NAUTILUS_COMPRESS_DIALOG_CONTROLLER (user_data);

    if (!gtk_toggle_button_get_active (toggle_button))
    {
        return;
    }

    update_selected_format (controller,
                            NAUTILUS_COMPRESSION_TAR_XZ);
}

static void
seven_zip_radio_button_on_toggled (GtkToggleButton *toggle_button,
                                   gpointer         user_data)
{
    NautilusCompressDialogController *controller;

    controller = NAUTILUS_COMPRESS_DIALOG_CONTROLLER (user_data);

    if (!gtk_toggle_button_get_active (toggle_button))
    {
        return;
    }

    update_selected_format (controller,
                            NAUTILUS_COMPRESSION_7ZIP);
}

NautilusCompressDialogController *
nautilus_compress_dialog_controller_new (GtkWindow         *parent_window,
                                         NautilusDirectory *destination_directory,
                                         gchar             *initial_name)
{
    NautilusCompressDialogController *self;
    g_autoptr (GtkBuilder) builder = NULL;
    GtkWidget *compress_dialog;
    GtkWidget *error_revealer;
    GtkWidget *error_label;
    GtkWidget *name_entry;
    GtkWidget *activate_button;
    GtkWidget *description_stack;
    GtkWidget *zip_radio_button;
    GtkWidget *tar_xz_radio_button;
    GtkWidget *seven_zip_radio_button;
    NautilusCompressionFormat format;

    builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-compress-dialog.ui");
    compress_dialog = GTK_WIDGET (gtk_builder_get_object (builder, "compress_dialog"));
    error_revealer = GTK_WIDGET (gtk_builder_get_object (builder, "error_revealer"));
    error_label = GTK_WIDGET (gtk_builder_get_object (builder, "error_label"));
    name_entry = GTK_WIDGET (gtk_builder_get_object (builder, "name_entry"));
    activate_button = GTK_WIDGET (gtk_builder_get_object (builder, "activate_button"));
    zip_radio_button = GTK_WIDGET (gtk_builder_get_object (builder, "zip_radio_button"));
    tar_xz_radio_button = GTK_WIDGET (gtk_builder_get_object (builder, "tar_xz_radio_button"));
    seven_zip_radio_button = GTK_WIDGET (gtk_builder_get_object (builder, "seven_zip_radio_button"));
    description_stack = GTK_WIDGET (gtk_builder_get_object (builder, "description_stack"));

    gtk_window_set_transient_for (GTK_WINDOW (compress_dialog),
                                  parent_window);

    self = g_object_new (NAUTILUS_TYPE_COMPRESS_DIALOG_CONTROLLER,
                         "error-revealer", error_revealer,
                         "error-label", error_label,
                         "name-entry", name_entry,
                         "activate-button", activate_button,
                         "containing-directory", destination_directory, NULL);

    self->compress_dialog = compress_dialog;
    self->zip_radio_button = zip_radio_button;
    self->tar_xz_radio_button = tar_xz_radio_button;
    self->seven_zip_radio_button = seven_zip_radio_button;
    self->description_stack = description_stack;
    self->name_entry = name_entry;

    self->response_handler_id = g_signal_connect (compress_dialog,
                                                  "response",
                                                  (GCallback) compress_dialog_controller_on_response,
                                                  self);

    gtk_builder_add_callback_symbols (builder,
                                      "zip_radio_button_on_toggled",
                                      G_CALLBACK (zip_radio_button_on_toggled),
                                      "tar_xz_radio_button_on_toggled",
                                      G_CALLBACK (tar_xz_radio_button_on_toggled),
                                      "seven_zip_radio_button_on_toggled",
                                      G_CALLBACK (seven_zip_radio_button_on_toggled),
                                      NULL);
    gtk_builder_connect_signals (builder, self);

    format = g_settings_get_enum (nautilus_compression_preferences,
                                  NAUTILUS_PREFERENCES_DEFAULT_COMPRESSION_FORMAT);

    update_selected_format (self, format);

    if (initial_name != NULL)
    {
        gtk_entry_set_text (GTK_ENTRY (name_entry), initial_name);
    }

    gtk_widget_show (compress_dialog);

    return self;
}

static void
nautilus_compress_dialog_controller_init (NautilusCompressDialogController *self)
{
}

static void
nautilus_compress_dialog_controller_finalize (GObject *object)
{
    NautilusCompressDialogController *self;

    self = NAUTILUS_COMPRESS_DIALOG_CONTROLLER (object);

    if (self->compress_dialog != NULL)
    {
        if (self->response_handler_id > 0)
        {
            g_signal_handler_disconnect (self->compress_dialog,
                                         self->response_handler_id);
            self->response_handler_id = 0;
        }
        gtk_widget_destroy (self->compress_dialog);
        self->compress_dialog = NULL;
    }

    G_OBJECT_CLASS (nautilus_compress_dialog_controller_parent_class)->finalize (object);
}

static void
nautilus_compress_dialog_controller_class_init (NautilusCompressDialogControllerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    NautilusFileNameWidgetControllerClass *parent_class = NAUTILUS_FILE_NAME_WIDGET_CONTROLLER_CLASS (klass);

    object_class->finalize = nautilus_compress_dialog_controller_finalize;

    parent_class->get_new_name = nautilus_compress_dialog_controller_get_new_name;
    parent_class->name_is_valid = nautilus_compress_dialog_controller_name_is_valid;
}
