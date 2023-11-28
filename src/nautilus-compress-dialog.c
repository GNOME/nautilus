/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gnome-autoar/gnome-autoar.h>

#include "nautilus-compress-dialog.h"

#include "nautilus-filename-validator.h"
#include "nautilus-global-preferences.h"

struct _NautilusCompressDialog
{
    AdwWindow parent_instance;

    NautilusFilenameValidator *validator;

    GtkWidget *name_entry;
    GtkWidget *extension_dropdown;
    GtkSizeGroup *extension_sizegroup;
    GtkWidget *passphrase_label;
    GtkWidget *passphrase_entry;
    GtkWidget *passphrase_confirm_label;
    GtkWidget *passphrase_confirm_entry;

    CompressCallback callback;
    gpointer callback_data;
};

G_DEFINE_TYPE (NautilusCompressDialog, nautilus_compress_dialog, ADW_TYPE_WINDOW);

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

static void
set_show_passphrase (NautilusCompressDialog *self,
                     gboolean                show_passphrase)
{
    gtk_widget_set_visible (self->passphrase_label, show_passphrase);
    gtk_widget_set_visible (self->passphrase_entry, show_passphrase);
    gtk_widget_set_visible (self->passphrase_confirm_label, show_passphrase);
    gtk_widget_set_visible (self->passphrase_confirm_entry, show_passphrase);
    if (!show_passphrase)
    {
        gtk_editable_set_text (GTK_EDITABLE (self->passphrase_entry), "");
        gtk_editable_set_text (GTK_EDITABLE (self->passphrase_confirm_entry), "");
    }
}

static void
update_selected_format (NautilusCompressDialog *self)
{
    NautilusCompressItem *item = gtk_drop_down_get_selected_item (GTK_DROP_DOWN (self->extension_dropdown));
    if (item == NULL)
    {
        return;
    }

    set_show_passphrase (self, item->format == NAUTILUS_COMPRESSION_ENCRYPTED_ZIP);

    g_settings_set_enum (nautilus_compression_preferences,
                         NAUTILUS_PREFERENCES_DEFAULT_COMPRESSION_FORMAT,
                         item->format);

    /* Since the extension changes when the button is toggled, force a
     * verification of the new file name.
     */
    nautilus_filename_validator_validate (self->validator);
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

static gboolean
make_opaque_iff_selected (GBinding     *binding,
                          const GValue *from_value,
                          GValue       *to_value,
                          gpointer      user_data)
{
    GtkListItem *listitem = GTK_LIST_ITEM (user_data);
    NautilusCompressItem *selected_item = NAUTILUS_COMPRESS_ITEM (g_value_get_object (from_value));
    gboolean is_selected = (selected_item != NULL &&
                            selected_item == gtk_list_item_get_item (listitem));
    gdouble opacity = (is_selected) ? 1.0 : 0.0;

    g_value_set_double (to_value, opacity);
    return TRUE;
}

static void
extension_dropdown_setup_item_full (GtkSignalListItemFactory *factory,
                                    GtkListItem              *item,
                                    gpointer                  user_data)
{
    NautilusCompressDialog *self = NAUTILUS_COMPRESS_DIALOG (user_data);
    GtkWidget *hbox, *vbox, *title, *subtitle, *checkmark;

    title = gtk_label_new ("");
    gtk_label_set_xalign (GTK_LABEL (title), 0.0);
    gtk_widget_set_halign (title, GTK_ALIGN_START);

    subtitle = gtk_label_new ("");
    gtk_label_set_xalign (GTK_LABEL (subtitle), 0.0);
    gtk_widget_add_css_class (subtitle, "dim-label");
    gtk_widget_add_css_class (subtitle, "caption");

    checkmark = gtk_image_new_from_icon_name ("object-select-symbolic");
    g_object_bind_property_full (self->extension_dropdown, "selected-item",
                                 checkmark, "opacity",
                                 G_BINDING_SYNC_CREATE,
                                 make_opaque_iff_selected, NULL,
                                 item, NULL);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_set_hexpand (vbox, TRUE);

    gtk_box_append (GTK_BOX (hbox), vbox);
    gtk_box_append (GTK_BOX (vbox), title);
    gtk_box_append (GTK_BOX (vbox), subtitle);
    gtk_box_append (GTK_BOX (hbox), checkmark);

    g_object_set_data (G_OBJECT (item), "title", title);
    g_object_set_data (G_OBJECT (item), "subtitle", subtitle);

    gtk_list_item_set_child (item, hbox);
}

static void
extension_dropdown_bind (GtkSignalListItemFactory *factory,
                         GtkListItem              *list_item,
                         gpointer                  user_data)
{
    NautilusCompressDialog *self = NAUTILUS_COMPRESS_DIALOG (user_data);
    GtkWidget *title, *subtitle;
    NautilusCompressItem *item;

    item = gtk_list_item_get_item (list_item);

    title = g_object_get_data (G_OBJECT (list_item), "title");
    subtitle = g_object_get_data (G_OBJECT (list_item), "subtitle");

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
}

static void
extension_dropdown_unbind (GtkSignalListItemFactory *factory,
                           GtkListItem              *item,
                           gpointer                  user_data)
{
    NautilusCompressDialog *self = NAUTILUS_COMPRESS_DIALOG (user_data);
    GtkWidget *title;

    if (self->extension_dropdown == NULL)
    {
        return;
    }

    title = g_object_get_data (G_OBJECT (item), "title");
    if (title)
    {
        gtk_widget_remove_css_class (title, "encrypted_zip");
    }
}

static gboolean
are_name_and_passphrase_ready (NautilusCompressDialog *self,
                               gboolean                filename_passed,
                               NautilusCompressItem   *selected_item,
                               const char             *passphrase,
                               const char             *passphrase_confirm)
{
    if (selected_item != NULL && selected_item->format == NAUTILUS_COMPRESSION_ENCRYPTED_ZIP &&
        (passphrase == NULL || passphrase[0] == '\0' || g_strcmp0 (passphrase, passphrase_confirm) != 0))
    {
        return FALSE;
    }
    else
    {
        return filename_passed;
    }
}

static void
extension_dropdown_setup (NautilusCompressDialog *self)
{
    GtkListItemFactory *factory, *list_factory;
    GListStore *store;
    NautilusCompressItem *item;
    NautilusCompressionFormat format;

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
    for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (store)); i++)
    {
        item = g_list_model_get_item (G_LIST_MODEL (store), i);
        if (item->format == format)
        {
            gtk_drop_down_set_selected (GTK_DROP_DOWN (self->extension_dropdown), i);
            set_show_passphrase (self, item->format == NAUTILUS_COMPRESSION_ENCRYPTED_ZIP);
            g_object_unref (item);
            break;
        }

        g_object_unref (item);
    }

    g_object_unref (store);
    g_object_unref (factory);
    g_object_unref (list_factory);
}

static void
on_name_accepted (NautilusCompressDialog *self)
{
    g_autofree char *name = nautilus_filename_validator_get_new_name (self->validator);
    const char *passphrase = gtk_editable_get_text (GTK_EDITABLE (self->passphrase_entry));

    self->callback (name, passphrase, self->callback_data);

    gtk_window_close (GTK_WINDOW (self));
}

static void
on_name_entry_activated (NautilusCompressDialog *self)
{
    if (gtk_widget_is_visible (self->passphrase_entry))
    {
        gtk_widget_grab_focus (self->passphrase_entry);
    }
    else
    {
        nautilus_filename_validator_validate (self->validator);
    }
}

static char *
maybe_append_extension (NautilusCompressDialog *self,
                        gchar                  *text,
                        NautilusCompressItem   *selected_item)
{
    g_autofree char *basename = g_strdup (text);

    if (basename == NULL)
    {
        return NULL;
    }

    g_strstrip (basename);

    if (selected_item != NULL && selected_item->extension != NULL &&
        !g_str_has_suffix (basename, selected_item->extension))
    {
        return g_strconcat (basename, selected_item->extension, NULL);
    }
    else
    {
        return g_steal_pointer (&basename);
    }
}

NautilusCompressDialog *
nautilus_compress_dialog_new (GtkWindow         *parent_window,
                              NautilusDirectory *destination_directory,
                              const char        *initial_name,
                              CompressCallback   callback,
                              gpointer           callback_data)
{
    NautilusCompressDialog *self = g_object_new (NAUTILUS_TYPE_COMPRESS_DIALOG,
                                                 "transient-for", parent_window,
                                                 NULL);

    nautilus_filename_validator_set_containing_directory (self->validator,
                                                          destination_directory);

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
nautilus_compress_dialog_init (NautilusCompressDialog *self)
{
    g_type_ensure (NAUTILUS_TYPE_FILENAME_VALIDATOR);
    gtk_widget_init_template (GTK_WIDGET (self));

    g_signal_handlers_block_by_func (self->extension_dropdown, update_selected_format, self);
    extension_dropdown_setup (self);
    g_signal_handlers_unblock_by_func (self->extension_dropdown, update_selected_format, self);
}

static void
nautilus_compress_dialog_dispose (GObject *object)
{
    gtk_widget_dispose_template (GTK_WIDGET (object), NAUTILUS_TYPE_COMPRESS_DIALOG);

    G_OBJECT_CLASS (nautilus_compress_dialog_parent_class)->dispose (object);
}

static void
nautilus_compress_dialog_finalize (GObject *object)
{
    G_OBJECT_CLASS (nautilus_compress_dialog_parent_class)->finalize (object);
}

static void
nautilus_compress_dialog_class_init (NautilusCompressDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = nautilus_compress_dialog_finalize;
    object_class->dispose = nautilus_compress_dialog_dispose;

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/nautilus/ui/nautilus-compress-dialog.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusCompressDialog, extension_dropdown);
    gtk_widget_class_bind_template_child (widget_class, NautilusCompressDialog, extension_sizegroup);
    gtk_widget_class_bind_template_child (widget_class, NautilusCompressDialog, name_entry);
    gtk_widget_class_bind_template_child (widget_class, NautilusCompressDialog, passphrase_confirm_entry);
    gtk_widget_class_bind_template_child (widget_class, NautilusCompressDialog, passphrase_confirm_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusCompressDialog, passphrase_entry);
    gtk_widget_class_bind_template_child (widget_class, NautilusCompressDialog, passphrase_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusCompressDialog, validator);

    gtk_widget_class_bind_template_callback (widget_class, update_selected_format);
    gtk_widget_class_bind_template_callback (widget_class, on_name_accepted);
    gtk_widget_class_bind_template_callback (widget_class, on_name_entry_activated);
    gtk_widget_class_bind_template_callback (widget_class, nautilus_filename_validator_try_accept);
    gtk_widget_class_bind_template_callback (widget_class, nautilus_filename_validator_validate);
    gtk_widget_class_bind_template_callback (widget_class, maybe_append_extension);
    gtk_widget_class_bind_template_callback (widget_class, are_name_and_passphrase_ready);
}
