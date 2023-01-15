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

    GtkImage *star;
};

G_DEFINE_TYPE (NautilusStarCell, nautilus_star_cell, NAUTILUS_TYPE_VIEW_CELL)

static void
on_star_click_released (GtkGestureClick *gesture,
                        gint             n_press,
                        gdouble          x,
                        gdouble          y,
                        gpointer         user_data)
{
    NautilusStarCell *self = user_data;
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
                                           &(GList){ file, NULL },
                                           NULL,
                                           NULL);
        gtk_widget_remove_css_class (GTK_WIDGET (self->star), "added");
    }
    else
    {
        nautilus_tag_manager_star_files (tag_manager,
                                         G_OBJECT (item),
                                         &(GList){ file, NULL },
                                         NULL,
                                         NULL);
        gtk_widget_add_css_class (GTK_WIDGET (self->star), "added");
    }

    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
update_star (GtkImage     *star,
             NautilusFile *file)
{
    gboolean is_starred;
    g_autofree gchar *file_uri = NULL;

    g_return_if_fail (NAUTILUS_IS_FILE (file));

    file_uri = nautilus_file_get_uri (file);
    is_starred = nautilus_tag_manager_file_is_starred (nautilus_tag_manager_get (),
                                                       file_uri);

    gtk_image_set_from_icon_name (star, is_starred ? "starred-symbolic" : "non-starred-symbolic");
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
    GtkGesture *gesture;

    /* Create star icon */
    star = gtk_image_new ();
    gtk_widget_set_halign (star, GTK_ALIGN_END);
    gtk_widget_set_valign (star, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class (star, "dim-label");
    gtk_widget_add_css_class (star, "star");
    adw_bin_set_child (ADW_BIN (self), star);
    self->star = GTK_IMAGE (star);

    /* Make it clickable */
    gesture = gtk_gesture_click_new ();
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_PRIMARY);
    g_signal_connect (gesture, "released", G_CALLBACK (on_star_click_released), self);
    gtk_widget_add_controller (star, GTK_EVENT_CONTROLLER (gesture));

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
