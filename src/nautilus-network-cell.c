/*
 * Copyright (C) 2024 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-network-cell.h"

#include <glib/gi18n.h>

#include "nautilus-directory.h"
#include "nautilus-file-utilities.h"
#include "nautilus-scheme.h"

struct _NautilusNetworkCell
{
    NautilusViewCell parent_instance;

    GSignalGroup *item_signal_group;

    GtkWidget *icon;
    GtkWidget *target_uri;
    GtkWidget *unmount_button;
};

G_DEFINE_TYPE (NautilusNetworkCell, nautilus_network_cell, NAUTILUS_TYPE_VIEW_CELL)

static void
update_labels (NautilusNetworkCell *self)
{
    g_autoptr (NautilusViewItem) item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));

    g_return_if_fail (item != NULL);

    NautilusFile *file = nautilus_view_item_get_file (item);
    g_autofree char *target_uri = nautilus_file_get_activation_uri (file);

    if (g_str_has_prefix (target_uri, SCHEME_COMPUTER ":///"))
    {
        /* Online accounts do not currently have a target URI. */
        g_set_str (&target_uri, _("Online Account"));
    }

    gtk_label_set_label (GTK_LABEL (self->target_uri), target_uri);
}

static void
update_icon (NautilusNetworkCell *self)
{
    g_autoptr (NautilusViewItem) item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));

    g_return_if_fail (item != NULL);

    g_autoptr (GIcon) icon = nautilus_file_get_gicon (nautilus_view_item_get_file (item),
                                                      NAUTILUS_FILE_ICON_FLAGS_NONE);

    gtk_image_set_from_gicon (GTK_IMAGE (self->icon), icon);
}

static void
on_file_changed (NautilusNetworkCell *self)
{
    g_autoptr (NautilusViewItem) item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));

    g_return_if_fail (item != NULL);

    update_icon (self);
    update_labels (self);

    NautilusFile *file = nautilus_view_item_get_file (item);

    gtk_widget_set_visible (self->unmount_button, nautilus_file_can_unmount (file));
}

static void
on_unmount_clicked (NautilusNetworkCell *self)
{
    /* Select item first, because view.unmount-volume acts on selection. */
    gtk_widget_activate_action (GTK_WIDGET (self), "listitem.select", "(bb)", FALSE, FALSE);
    gtk_widget_activate_action (GTK_WIDGET (self), "view.unmount-volume", NULL);
}

static void
nautilus_network_cell_init (NautilusNetworkCell *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    /* Connect automatically to an item. */
    self->item_signal_group = g_signal_group_new (NAUTILUS_TYPE_VIEW_ITEM);
    g_signal_group_connect_swapped (self->item_signal_group, "notify::loading",
                                    (GCallback) on_file_changed, self);
    g_signal_group_connect_swapped (self->item_signal_group, "file-changed",
                                    (GCallback) on_file_changed, self);
    g_signal_connect_object (self->item_signal_group, "bind",
                             (GCallback) on_file_changed, self,
                             G_CONNECT_SWAPPED);

    g_object_bind_property (self, "item",
                            self->item_signal_group, "target",
                            G_BINDING_SYNC_CREATE);
}

static void
nautilus_network_cell_dispose (GObject *object)
{
    NautilusNetworkCell *self = (NautilusNetworkCell *) object;

    gtk_widget_dispose_template (GTK_WIDGET (self), NAUTILUS_TYPE_NETWORK_CELL);

    G_OBJECT_CLASS (nautilus_network_cell_parent_class)->dispose (object);
}

static void
nautilus_network_cell_finalize (GObject *object)
{
    NautilusNetworkCell *self = (NautilusNetworkCell *) object;

    g_clear_object (&self->item_signal_group);
    G_OBJECT_CLASS (nautilus_network_cell_parent_class)->finalize (object);
}

static void
nautilus_network_cell_class_init (NautilusNetworkCellClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = nautilus_network_cell_dispose;
    object_class->finalize = nautilus_network_cell_finalize;

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-network-cell.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusNetworkCell, icon);
    gtk_widget_class_bind_template_child (widget_class, NautilusNetworkCell, target_uri);
    gtk_widget_class_bind_template_child (widget_class, NautilusNetworkCell, unmount_button);
    gtk_widget_class_bind_template_callback (widget_class, on_unmount_clicked);
}

NautilusViewCell *
nautilus_network_cell_new (NautilusListBase *view)
{
    return NAUTILUS_VIEW_CELL (g_object_new (NAUTILUS_TYPE_NETWORK_CELL,
                                             "view", view,
                                             NULL));
}
