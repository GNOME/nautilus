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

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gnome-autoar/gnome-autoar.h>

#include "nautilus-compress-dialog-controller.h"

#include "nautilus-filename-message.h"
#include "nautilus-global-preferences.h"

struct _NautilusCompressDialogController
{
    NautilusFileNameWidgetController parent_instance;

    GtkWidget *compress_dialog;
    GtkWidget *activate_button;
    GtkRevealer *error_revealer;
    GtkWidget *error_label;
    GtkWidget *name_entry;
    GtkWidget *extension_dropdown;
    GtkSizeGroup *extension_sizegroup;
    GtkWidget *passphrase_label;
    GtkWidget *passphrase_entry;
    GtkWidget *passphrase_confirm_label;
    GtkWidget *passphrase_confirm_entry;

    NautilusDirectory *containing_directory;

    const char *extension;
    gchar *passphrase;
    gchar *passphrase_confirm;

    gulong response_handler_id;
};

G_DEFINE_TYPE (NautilusCompressDialogController, nautilus_compress_dialog_controller, NAUTILUS_TYPE_FILE_NAME_WIDGET_CONTROLLER);

#define NAUTILUS_TYPE_COMPRESS_ITEM (nautilus_compress_item_get_type ())
G_DECLARE_FINAL_TYPE (NautilusCompressItem, nautilus_compress_item, NAUTILUS, COMPRESS_ITEM, GObject)

struct _NautilusCompressItem
{
    GObject parent_instance;
    NautilusCompressionFormat format;
    char *extension;
    char *description;
};

G_DEFINE_TYPE (NautilusCompressItem, nautilus_compress_item, G_TYPE_OBJECT);

static void
nautilus_compress_item_init (NautilusCompressItem *item)
{
}

static void
nautilus_compress_item_finalize (GObject *object)
{
    NautilusCompressItem *item = NAUTILUS_COMPRESS_ITEM (object);

    g_free (item->extension);
    g_free (item->description);

    G_OBJECT_CLASS (nautilus_compress_item_parent_class)->finalize (object);
}

static void
nautilus_compress_item_class_init (NautilusCompressItemClass *class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (class);

    object_class->finalize = nautilus_compress_item_finalize;
}

static NautilusCompressItem *
nautilus_compress_item_new (NautilusCompressionFormat  format,
                            const char                *extension,
                            const char                *description)
{
    NautilusCompressItem *item = g_object_new (NAUTILUS_TYPE_COMPRESS_ITEM, NULL);

    item->format = format;
    item->extension = g_strdup (extension);
    item->description = g_strdup (description);

    return item;
}

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

static gboolean
update_name (NautilusFileNameWidgetController *controller)
{
    NautilusCompressDialogController *self = NAUTILUS_COMPRESS_DIALOG_CONTROLLER (controller);
    g_autofree char *name = nautilus_compress_dialog_controller_get_new_name (controller);
    NautilusFileNameMessage message = nautilus_filename_message_from_name (name,
                                                                           self->containing_directory,
                                                                           NULL);
    gboolean is_valid = nautilus_filename_message_is_valid (message);
    const char *error_message = nautilus_filename_message_archive_error (message);

    gtk_label_set_label (GTK_LABEL (self->error_label), error_message);
    gtk_revealer_set_reveal_child (self->error_revealer, error_message != NULL);
    gtk_widget_set_sensitive (self->activate_button, is_valid);

    return is_valid;
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
update_selected_format (NautilusCompressDialogController *self)
{
    gboolean show_passphrase = FALSE;
    guint selected;
    GListModel *model;
    NautilusCompressItem *item;

    selected = gtk_drop_down_get_selected (GTK_DROP_DOWN (self->extension_dropdown));
    if (selected == GTK_INVALID_LIST_POSITION)
    {
        return;
    }

    model = gtk_drop_down_get_model (GTK_DROP_DOWN (self->extension_dropdown));
    item = g_list_model_get_item (model, selected);
    if (item == NULL)
    {
        return;
    }

    if (item->format == NAUTILUS_COMPRESSION_ENCRYPTED_ZIP)
    {
        show_passphrase = TRUE;
    }

    self->extension = item->extension;

    gtk_widget_set_visible (self->passphrase_label, show_passphrase);
    gtk_widget_set_visible (self->passphrase_entry, show_passphrase);
    gtk_widget_set_visible (self->passphrase_confirm_label, show_passphrase);
    gtk_widget_set_visible (self->passphrase_confirm_entry, show_passphrase);
    if (!show_passphrase)
    {
        gtk_editable_set_text (GTK_EDITABLE (self->passphrase_entry), "");
        gtk_entry_set_visibility (GTK_ENTRY (self->passphrase_entry), FALSE);
        gtk_entry_set_icon_from_icon_name (GTK_ENTRY (self->passphrase_entry),
                                           GTK_ENTRY_ICON_SECONDARY,
                                           "view-conceal");

        gtk_editable_set_text (GTK_EDITABLE (self->passphrase_confirm_entry), "");
        gtk_entry_set_visibility (GTK_ENTRY (self->passphrase_confirm_entry), FALSE);
        gtk_entry_set_icon_from_icon_name (GTK_ENTRY (self->passphrase_confirm_entry),
                                           GTK_ENTRY_ICON_SECONDARY,
                                           "view-conceal");
    }

    g_settings_set_enum (nautilus_compression_preferences,
                         NAUTILUS_PREFERENCES_DEFAULT_COMPRESSION_FORMAT,
                         item->format);

    /* Since the extension changes when the button is toggled, force a
     * verification of the new file name by simulating an entry change
     */
    gtk_widget_set_sensitive (self->activate_button, FALSE);
    g_signal_emit_by_name (self->name_entry, "changed");
}

static void
extension_dropdown_setup_item (GtkSignalListItemFactory *factory,
                               GtkListItem              *item,
                               gpointer                  user_data)
{
    GtkWidget *title;

    title = gtk_label_new ("");
    gtk_label_set_xalign (GTK_LABEL (title), 0.0);

    g_object_set_data (G_OBJECT (item), "title", title);
    gtk_list_item_set_child (item, title);
}


static void
extension_dropdown_setup_item_full (GtkSignalListItemFactory *factory,
                                    GtkListItem              *item,
                                    gpointer                  user_data)
{
    GtkWidget *hbox, *vbox, *title, *subtitle, *checkmark;

    title = gtk_label_new ("");
    gtk_label_set_xalign (GTK_LABEL (title), 0.0);
    gtk_widget_set_halign (title, GTK_ALIGN_START);

    subtitle = gtk_label_new ("");
    gtk_label_set_xalign (GTK_LABEL (subtitle), 0.0);
    gtk_widget_add_css_class (subtitle, "dim-label");
    gtk_widget_add_css_class (subtitle, "caption");

    checkmark = gtk_image_new_from_icon_name ("object-select-symbolic");

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_set_hexpand (vbox, TRUE);

    gtk_box_append (GTK_BOX (hbox), vbox);
    gtk_box_append (GTK_BOX (vbox), title);
    gtk_box_append (GTK_BOX (vbox), subtitle);
    gtk_box_append (GTK_BOX (hbox), checkmark);

    g_object_set_data (G_OBJECT (item), "title", title);
    g_object_set_data (G_OBJECT (item), "subtitle", subtitle);
    g_object_set_data (G_OBJECT (item), "checkmark", checkmark);

    gtk_list_item_set_child (item, hbox);
}

static void
extension_dropdown_on_selected_item_notify (GtkDropDown *dropdown,
                                            GParamSpec  *pspec,
                                            GtkListItem *item)
{
    GtkWidget *checkmark;

    checkmark = g_object_get_data (G_OBJECT (item), "checkmark");

    if (gtk_drop_down_get_selected_item (dropdown) == gtk_list_item_get_item (item))
    {
        gtk_widget_set_opacity (checkmark, 1.0);
    }
    else
    {
        gtk_widget_set_opacity (checkmark, 0.0);
    }
}

static void
extension_dropdown_bind (GtkSignalListItemFactory *factory,
                         GtkListItem              *list_item,
                         gpointer                  user_data)
{
    NautilusCompressDialogController *self;
    GtkWidget *title, *subtitle, *checkmark;
    NautilusCompressItem *item;

    self = NAUTILUS_COMPRESS_DIALOG_CONTROLLER (user_data);
    item = gtk_list_item_get_item (list_item);

    title = g_object_get_data (G_OBJECT (list_item), "title");
    subtitle = g_object_get_data (G_OBJECT (list_item), "subtitle");
    checkmark = g_object_get_data (G_OBJECT (list_item), "checkmark");

    gtk_label_set_label (GTK_LABEL (title), item->extension);
    gtk_size_group_add_widget (self->extension_sizegroup, title);

    if (item->format == NAUTILUS_COMPRESSION_ENCRYPTED_ZIP)
    {
        gtk_widget_add_css_class (title, "encrypted_zip");
    }

    if (subtitle)
    {
        gtk_label_set_label (GTK_LABEL (subtitle), item->description);
    }

    if (checkmark)
    {
        g_signal_connect (self->extension_dropdown,
                          "notify::selected-item",
                          G_CALLBACK (extension_dropdown_on_selected_item_notify),
                          list_item);
        extension_dropdown_on_selected_item_notify (GTK_DROP_DOWN (self->extension_dropdown),
                                                    NULL,
                                                    list_item);
    }
}

static void
extension_dropdown_unbind (GtkSignalListItemFactory *factory,
                           GtkListItem              *item,
                           gpointer                  user_data)
{
    NautilusCompressDialogController *self;
    GtkWidget *title;

    self = NAUTILUS_COMPRESS_DIALOG_CONTROLLER (user_data);
    g_signal_handlers_disconnect_by_func (self->extension_dropdown,
                                          extension_dropdown_on_selected_item_notify,
                                          item);

    title = g_object_get_data (G_OBJECT (item), "title");
    if (title)
    {
        gtk_widget_remove_css_class (title, "encrypted_zip");
    }
}

static void
update_passphrase (NautilusCompressDialogController *self,
                   gchar                            *passphrase,
                   GtkEditable                      *editable)
{
    const gchar *error_message;

    g_free (passphrase);
    passphrase = g_strdup (gtk_editable_get_text (editable));

    /* Simulate a change of the name_entry to ensure the correct sensitivity of
     * the activate_button, but only if the name_entry is valid in order to
     * avoid changes of the error_revealer.
     */
    error_message = gtk_label_get_text (GTK_LABEL (self->error_label));
    if (error_message[0] == '\0')
    {
        gtk_widget_set_sensitive (self->activate_button, FALSE);
        g_signal_emit_by_name (self->name_entry, "changed");
    }
}

static void
passphrase_entry_on_changed (GtkEditable *editable,
                             gpointer     user_data)
{
    NautilusCompressDialogController *self = NAUTILUS_COMPRESS_DIALOG_CONTROLLER (user_data);

    update_passphrase (self, self->passphrase, editable);
}

static void
passphrase_confirm_entry_on_changed (GtkEditable *editable,
                                     gpointer     user_data)
{
    NautilusCompressDialogController *self = NAUTILUS_COMPRESS_DIALOG_CONTROLLER (user_data);

    update_passphrase (self, self->passphrase_confirm, editable);
}

static void
passphrase_entry_on_icon_press (GtkEntry             *entry,
                                GtkEntryIconPosition  icon_pos,
                                gpointer              user_data)
{
    gboolean visibility = gtk_entry_get_visibility (GTK_ENTRY (entry));

    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
                                       GTK_ENTRY_ICON_SECONDARY,
                                       visibility ? "view-conceal" : "view-reveal");
    gtk_entry_set_visibility (GTK_ENTRY (entry), !visibility);
}

static void
activate_button_on_sensitive_notify (GObject    *gobject,
                                     GParamSpec *pspec,
                                     gpointer    user_data)
{
    NautilusCompressDialogController *self;
    NautilusCompressionFormat format;

    self = NAUTILUS_COMPRESS_DIALOG_CONTROLLER (user_data);
    format = g_settings_get_enum (nautilus_compression_preferences,
                                  NAUTILUS_PREFERENCES_DEFAULT_COMPRESSION_FORMAT);
    if (format == NAUTILUS_COMPRESSION_ENCRYPTED_ZIP &&
        (self->passphrase == NULL || self->passphrase[0] == '\0' || g_strcmp0 (self->passphrase, self->passphrase_confirm) != 0))
    {
        /* Reset sensitivity of the activate_button if password is not set. */
        gtk_widget_set_sensitive (self->activate_button, FALSE);
    }
}

static void
extension_dropdown_setup (NautilusCompressDialogController *self)
{
    GtkListItemFactory *factory, *list_factory;
    GListStore *store;
    NautilusCompressItem *item;
    NautilusCompressionFormat format;
    gint i;

    store = g_list_store_new (NAUTILUS_TYPE_COMPRESS_ITEM);
    item = nautilus_compress_item_new (NAUTILUS_COMPRESSION_ZIP,
                                       ".zip",
                                       _("Compatible with all operating systems."));
    g_list_store_append (store, item);
    g_object_unref (item);
    item = nautilus_compress_item_new (NAUTILUS_COMPRESSION_ENCRYPTED_ZIP,
                                       ".zip",
                                       _("Password protected .zip, must be installed on Windows and Mac."));
    g_list_store_append (store, item);
    g_object_unref (item);
    item = nautilus_compress_item_new (NAUTILUS_COMPRESSION_TAR_XZ,
                                       ".tar.xz",
                                       _("Smaller archives but Linux and Mac only."));
    g_list_store_append (store, item);
    g_object_unref (item);
    item = nautilus_compress_item_new (NAUTILUS_COMPRESSION_7ZIP,
                                       ".7z",
                                       _("Smaller archives but must be installed on Windows and Mac."));
    g_list_store_append (store, item);
    g_object_unref (item);

    factory = gtk_signal_list_item_factory_new ();
    g_signal_connect_object (factory, "setup",
                             G_CALLBACK (extension_dropdown_setup_item), self, 0);
    g_signal_connect_object (factory, "bind",
                             G_CALLBACK (extension_dropdown_bind), self, 0);
    g_signal_connect_object (factory, "unbind",
                             G_CALLBACK (extension_dropdown_unbind), self, 0);

    list_factory = gtk_signal_list_item_factory_new ();
    g_signal_connect_object (list_factory, "setup",
                             G_CALLBACK (extension_dropdown_setup_item_full), self, 0);
    g_signal_connect_object (list_factory, "bind",
                             G_CALLBACK (extension_dropdown_bind), self, 0);
    g_signal_connect_object (list_factory, "unbind",
                             G_CALLBACK (extension_dropdown_unbind), self, 0);

    gtk_drop_down_set_factory (GTK_DROP_DOWN (self->extension_dropdown), factory);
    gtk_drop_down_set_list_factory (GTK_DROP_DOWN (self->extension_dropdown), list_factory);
    gtk_drop_down_set_model (GTK_DROP_DOWN (self->extension_dropdown), G_LIST_MODEL (store));

    format = g_settings_get_enum (nautilus_compression_preferences,
                                  NAUTILUS_PREFERENCES_DEFAULT_COMPRESSION_FORMAT);
    for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (store)); i++)
    {
        item = g_list_model_get_item (G_LIST_MODEL (store), i);
        if (item->format == format)
        {
            gtk_drop_down_set_selected (GTK_DROP_DOWN (self->extension_dropdown), i);
            update_selected_format (self);
            g_object_unref (item);
            break;
        }

        g_object_unref (item);
    }

    g_object_unref (store);
    g_object_unref (factory);
    g_object_unref (list_factory);
}

NautilusCompressDialogController *
nautilus_compress_dialog_controller_new (GtkWindow         *parent_window,
                                         NautilusDirectory *destination_directory,
                                         const char        *initial_name)
{
    NautilusCompressDialogController *self;
    g_autoptr (GtkBuilder) builder = NULL;
    GtkWidget *compress_dialog;
    GtkWidget *error_revealer;
    GtkWidget *error_label;
    GtkWidget *name_entry;
    GtkWidget *activate_button;
    GtkWidget *extension_dropdown;
    GtkSizeGroup *extension_sizegroup;
    GtkWidget *passphrase_label;
    GtkWidget *passphrase_entry;
    GtkWidget *passphrase_confirm_label;
    GtkWidget *passphrase_confirm_entry;

    builder = gtk_builder_new_from_resource ("/org/gnome/nautilus/ui/nautilus-compress-dialog.ui");
    compress_dialog = GTK_WIDGET (gtk_builder_get_object (builder, "compress_dialog"));
    error_revealer = GTK_WIDGET (gtk_builder_get_object (builder, "error_revealer"));
    error_label = GTK_WIDGET (gtk_builder_get_object (builder, "error_label"));
    name_entry = GTK_WIDGET (gtk_builder_get_object (builder, "name_entry"));
    activate_button = GTK_WIDGET (gtk_builder_get_object (builder, "activate_button"));
    extension_dropdown = GTK_WIDGET (gtk_builder_get_object (builder, "extension_dropdown"));
    extension_sizegroup = GTK_SIZE_GROUP (gtk_builder_get_object (builder, "extension_sizegroup"));
    passphrase_label = GTK_WIDGET (gtk_builder_get_object (builder, "passphrase_label"));
    passphrase_entry = GTK_WIDGET (gtk_builder_get_object (builder, "passphrase_entry"));
    passphrase_confirm_label = GTK_WIDGET (gtk_builder_get_object (builder, "passphrase_confirm_label"));
    passphrase_confirm_entry = GTK_WIDGET (gtk_builder_get_object (builder, "passphrase_confirm_entry"));

    gtk_window_set_transient_for (GTK_WINDOW (compress_dialog),
                                  parent_window);

    self = g_object_new (NAUTILUS_TYPE_COMPRESS_DIALOG_CONTROLLER,
                         "error-revealer", error_revealer,
                         "error-label", error_label,
                         "name-entry", name_entry,
                         "activate-button", activate_button,
                         "containing-directory", destination_directory, NULL);

    self->compress_dialog = compress_dialog;
    self->activate_button = activate_button;
    self->error_revealer = GTK_REVEALER (error_revealer);
    self->error_label = error_label;
    self->name_entry = name_entry;
    self->extension_dropdown = extension_dropdown;
    self->extension_sizegroup = extension_sizegroup;
    self->passphrase_label = passphrase_label;
    self->passphrase_entry = passphrase_entry;
    self->passphrase_confirm_label = passphrase_confirm_label;
    self->passphrase_confirm_entry = passphrase_confirm_entry;
    self->containing_directory = nautilus_directory_ref (destination_directory);

    extension_dropdown_setup (self);

    self->response_handler_id = g_signal_connect (compress_dialog,
                                                  "response",
                                                  (GCallback) compress_dialog_controller_on_response,
                                                  self);

    g_signal_connect (self->passphrase_entry, "changed",
                      G_CALLBACK (passphrase_entry_on_changed), self);
    g_signal_connect (self->passphrase_entry, "icon-press",
                      G_CALLBACK (passphrase_entry_on_icon_press), self);
    g_signal_connect (self->passphrase_confirm_entry, "changed",
                      G_CALLBACK (passphrase_confirm_entry_on_changed), self);
    g_signal_connect (self->passphrase_confirm_entry, "icon-press",
                      G_CALLBACK (passphrase_entry_on_icon_press), self);
    g_signal_connect (self->activate_button, "notify::sensitive",
                      G_CALLBACK (activate_button_on_sensitive_notify), self);
    g_signal_connect_swapped (self->extension_dropdown, "notify::selected-item",
                              G_CALLBACK (update_selected_format), self);

    if (initial_name != NULL)
    {
        gtk_editable_set_text (GTK_EDITABLE (name_entry), initial_name);
    }

    gtk_window_present (GTK_WINDOW (compress_dialog));

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
        g_clear_signal_handler (&self->response_handler_id, self->compress_dialog);
        gtk_window_destroy (GTK_WINDOW (self->compress_dialog));
        self->compress_dialog = NULL;
    }

    g_clear_object (&self->containing_directory);
    g_free (self->passphrase);

    G_OBJECT_CLASS (nautilus_compress_dialog_controller_parent_class)->finalize (object);
}

static void
nautilus_compress_dialog_controller_class_init (NautilusCompressDialogControllerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    NautilusFileNameWidgetControllerClass *parent_class = NAUTILUS_FILE_NAME_WIDGET_CONTROLLER_CLASS (klass);

    object_class->finalize = nautilus_compress_dialog_controller_finalize;

    parent_class->get_new_name = nautilus_compress_dialog_controller_get_new_name;
    parent_class->update_name = update_name;
}

const gchar *
nautilus_compress_dialog_controller_get_passphrase (NautilusCompressDialogController *self)
{
    return self->passphrase;
}
