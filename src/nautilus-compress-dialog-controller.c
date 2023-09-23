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
    AdwWindow parent_instance;

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

    CompressCallback callback;
    gpointer callback_data;
    gboolean should_compress;

    const char *extension;
    gchar *passphrase;
    gchar *passphrase_confirm;
};

G_DEFINE_TYPE (NautilusCompressDialogController, nautilus_compress_dialog_controller, ADW_TYPE_WINDOW);

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

static char *
nautilus_compress_dialog_controller_get_new_name (NautilusCompressDialogController *self)
{
    g_autofree gchar *basename = g_strstrip (g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->name_entry))));

    if (basename == NULL || basename[0] == '\0' || g_str_has_suffix (basename, self->extension))
    {
        return g_steal_pointer (&basename);
    }

    return g_strconcat (basename, self->extension, NULL);
}

static gboolean
is_encryption_ok (NautilusCompressDialogController *self)
{
    NautilusCompressionFormat format =
        g_settings_get_enum (nautilus_compression_preferences,
                             NAUTILUS_PREFERENCES_DEFAULT_COMPRESSION_FORMAT);

    return format != NAUTILUS_COMPRESSION_ENCRYPTED_ZIP ||
           (self->passphrase != NULL && self->passphrase[0] != '\0' &&
            g_strcmp0 (self->passphrase, self->passphrase_confirm) == 0);
}

static void
update_state (NautilusDirectory *,
              GList *,
              gpointer user_data)
{
    NautilusCompressDialogController *self = NAUTILUS_COMPRESS_DIALOG_CONTROLLER (user_data);
    g_autofree char *name = nautilus_compress_dialog_controller_get_new_name (self);
    NautilusFileNameMessage message = nautilus_filename_message_from_name (name,
                                                                           self->containing_directory,
                                                                           NULL);
    gboolean is_valid = nautilus_filename_message_is_valid (message) &&
                        is_encryption_ok (self);
    const char *error_message = nautilus_filename_message_archive_error (message);

    gtk_label_set_label (GTK_LABEL (self->error_label), error_message);
    gtk_revealer_set_reveal_child (self->error_revealer, error_message != NULL);
    gtk_widget_set_sensitive (self->activate_button, is_valid);

    if (self->should_compress && is_valid)
    {
        self->callback (name, self->passphrase, self->callback_data);
    }
    else
    {
        self->should_compress = FALSE;
    }
}

static void
check_state (NautilusCompressDialogController *self)
{
    nautilus_directory_call_when_ready (self->containing_directory,
                                        NAUTILUS_FILE_ATTRIBUTE_INFO,
                                        TRUE,
                                        update_state,
                                        self);
}

static void
on_name_activated (NautilusCompressDialogController *self)
{
    if (is_encryption_ok (self))
    {
        self->should_compress = TRUE;
        check_state (self);
    }
    else
    {
        gtk_widget_grab_focus (self->passphrase_entry);
    }
}

static void
try_accept_name (NautilusCompressDialogController *self)
{
    self->should_compress = TRUE;
    check_state (self);
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
        gtk_editable_set_text (GTK_EDITABLE (self->passphrase_confirm_entry), "");
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

    if (self->extension_dropdown == NULL)
    {
        return;
    }

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
on_passphrase_changed (NautilusCompressDialogController *self,
                       GtkEditable                      *editable)
{
    g_free (self->passphrase);
    self->passphrase = g_strdup (gtk_editable_get_text (editable));
    check_state (self);
}

static void
on_passphrase_confirm_changed (NautilusCompressDialogController *self,
                               GtkEditable                      *editable)
{
    g_free (self->passphrase_confirm);
    self->passphrase_confirm = g_strdup (gtk_editable_get_text (editable));
    check_state (self);
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
                                         gchar             *initial_name,
                                         CompressCallback   callback,
                                         gpointer           callback_data)
{
    NautilusCompressDialogController *self = g_object_new (NAUTILUS_TYPE_COMPRESS_DIALOG_CONTROLLER, NULL);

    gtk_window_set_transient_for (GTK_WINDOW (self), parent_window);

    self->containing_directory = nautilus_directory_ref (destination_directory);
    self->callback = callback;
    self->callback_data = callback_data;

    extension_dropdown_setup (self);

    if (initial_name != NULL)
    {
        gtk_editable_set_text (GTK_EDITABLE (self->name_entry), initial_name);
    }
    gtk_widget_grab_focus (self->name_entry);

    gtk_window_present (GTK_WINDOW (self));

    return self;
}

static void
nautilus_compress_dialog_controller_init (NautilusCompressDialogController *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}

static void
compress_dialog_dispose (GObject *object)
{
    gtk_widget_dispose_template (GTK_WIDGET (object), NAUTILUS_TYPE_COMPRESS_DIALOG_CONTROLLER);

    G_OBJECT_CLASS (nautilus_compress_dialog_controller_parent_class)->dispose (object);
}

static void
nautilus_compress_dialog_controller_finalize (GObject *object)
{
    NautilusCompressDialogController *self;

    self = NAUTILUS_COMPRESS_DIALOG_CONTROLLER (object);

    g_clear_object (&self->containing_directory);
    g_free (self->passphrase);

    G_OBJECT_CLASS (nautilus_compress_dialog_controller_parent_class)->finalize (object);
}

static void
nautilus_compress_dialog_controller_class_init (NautilusCompressDialogControllerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = nautilus_compress_dialog_controller_finalize;
    object_class->dispose = compress_dialog_dispose;

    gtk_widget_class_set_template_from_resource (
        widget_class, "/org/gnome/nautilus/ui/nautilus-compress-dialog.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusCompressDialogController, activate_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusCompressDialogController, error_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusCompressDialogController, error_revealer);
    gtk_widget_class_bind_template_child (widget_class, NautilusCompressDialogController, extension_dropdown);
    gtk_widget_class_bind_template_child (widget_class, NautilusCompressDialogController, extension_sizegroup);
    gtk_widget_class_bind_template_child (widget_class, NautilusCompressDialogController, name_entry);
    gtk_widget_class_bind_template_child (widget_class, NautilusCompressDialogController, passphrase_confirm_entry);
    gtk_widget_class_bind_template_child (widget_class, NautilusCompressDialogController, passphrase_confirm_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusCompressDialogController, passphrase_entry);
    gtk_widget_class_bind_template_child (widget_class, NautilusCompressDialogController, passphrase_label);

    gtk_widget_class_bind_template_callback (widget_class, check_state);
    gtk_widget_class_bind_template_callback (widget_class, on_name_activated);
    gtk_widget_class_bind_template_callback (widget_class, on_passphrase_changed);
    gtk_widget_class_bind_template_callback (widget_class, on_passphrase_confirm_changed);
    gtk_widget_class_bind_template_callback (widget_class, try_accept_name);
    gtk_widget_class_bind_template_callback (widget_class, update_selected_format);
}
