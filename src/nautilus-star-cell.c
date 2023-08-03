/*
 * Copyright (C) 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-star-cell.h"
#include "nautilus-tag-manager.h"

struct _NautilusStarCell
{
    NautilusViewCell parent_instance;

    GSignalGroup *item_signal_group;

    GtkButton *star;
};

G_DEFINE_TYPE (NautilusStarCell, nautilus_star_cell, NAUTILUS_TYPE_VIEW_CELL)

static void
toggle_star (NautilusStarCell *self)
{
    NautilusTagManager *tag_manager = nautilus_tag_manager_get ();
    g_autoptr (NautilusViewItem) item = NULL;
    NautilusFile *file;
    g_autofree gchar *uri = NULL;

    item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    g_return_if_fail (item != NULL);
    file = nautilus_view_item_get_file (item);
    uri = nautilus_file_get_uri (file);

    if (nautilus_tag_manager_file_is_starred (tag_manager, uri))
    {
        nautilus_tag_manager_unstar_files (tag_manager,
                                           G_OBJECT (item),
                                           &(GList){ .data = file },
                                           NULL,
                                           NULL);
        gtk_widget_remove_css_class (GTK_WIDGET (self->star), "added");
    }
    else
    {
        nautilus_tag_manager_star_files (tag_manager,
                                         G_OBJECT (item),
                                         &(GList){ .data = file },
                                         NULL,
                                         NULL);
        gtk_widget_add_css_class (GTK_WIDGET (self->star), "added");
    }
}

static void
update_star (GtkButton    *star,
             NautilusFile *file)
{
    gboolean is_starred;
    g_autofree gchar *file_uri = NULL;

    g_return_if_fail (NAUTILUS_IS_FILE (file));

    file_uri = nautilus_file_get_uri (file);
    is_starred = nautilus_tag_manager_file_is_starred (nautilus_tag_manager_get (),
                                                       file_uri);

    gtk_button_set_icon_name (star, is_starred ? "starred-symbolic" : "non-starred-symbolic");
}

static void
on_file_changed (NautilusStarCell *self)
{
    g_autoptr (NautilusViewItem) item = NULL;
    NautilusFile *file;

    item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    g_return_if_fail (item != NULL);
    file = nautilus_view_item_get_file (item);

    update_star (self->star, file);
}

static void
on_starred_changed (NautilusTagManager *tag_manager,
                    GList              *changed_files,
                    gpointer            user_data)
{
    NautilusStarCell *self = user_data;
    g_autoptr (NautilusViewItem) item = NULL;
    NautilusFile *file;

    item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    if (item == NULL)
    {
        return;
    }

    file = nautilus_view_item_get_file (item);
    if (g_list_find (changed_files, file))
    {
        update_star (self->star, file);
    }
}

static void
nautilus_star_cell_init (NautilusStarCell *self)
{
    GtkWidget *star;

    /* Create star icon */
    star = gtk_button_new ();
    gtk_widget_set_halign (star, GTK_ALIGN_END);
    gtk_widget_set_valign (star, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class (star, "dim-label");
    gtk_widget_add_css_class (star, "star");
    gtk_widget_add_css_class (star, "flat");
    gtk_widget_add_css_class (star, "circular");
    adw_bin_set_child (ADW_BIN (self), star);
    self->star = GTK_BUTTON (star);

    g_signal_connect_swapped (self->star, "clicked", G_CALLBACK (toggle_star), self);

    /* Update on tag changes */
    g_signal_connect_object (nautilus_tag_manager_get (), "starred-changed",
                             G_CALLBACK (on_starred_changed), self, 0);

    /* Connect automatically to an item. */
    self->item_signal_group = g_signal_group_new (NAUTILUS_TYPE_VIEW_ITEM);
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
nautilus_star_cell_finalize (GObject *object)
{
    NautilusStarCell *self = (NautilusStarCell *) object;

    g_object_unref (self->item_signal_group);
    G_OBJECT_CLASS (nautilus_star_cell_parent_class)->finalize (object);
}

static void
nautilus_star_cell_class_init (NautilusStarCellClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = nautilus_star_cell_finalize;
}

NautilusViewCell *
nautilus_star_cell_new (NautilusListBase *view)
{
    return NAUTILUS_VIEW_CELL (g_object_new (NAUTILUS_TYPE_STAR_CELL,
                                             "view", view,
                                             NULL));
}
