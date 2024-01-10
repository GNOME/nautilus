/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-name-cell.h"
#include "nautilus-file-utilities.h"

#define LOADING_TIMEOUT_SECONDS 1

struct _NautilusNameCell
{
    NautilusViewCell parent_instance;

    GSignalGroup *item_signal_group;

    GQuark path_attribute_q;
    GFile *file_path_base_location;

    GtkWidget *expander;
    GtkWidget *content;
    GtkWidget *fixed_height_box;
    GtkWidget *spinner;
    GtkWidget *icon;
    GtkWidget *label;
    GtkWidget *emblems_box;
    GtkWidget *snippet_button;
    GtkWidget *snippet;
    GtkWidget *path;

    gboolean show_snippet;
    guint loading_timeout_id;
};

G_DEFINE_TYPE (NautilusNameCell, nautilus_name_cell, NAUTILUS_TYPE_VIEW_CELL)

static gchar *
get_path_text (NautilusFile *file,
               GQuark        path_attribute_q,
               GFile        *base_location)
{
    g_autofree gchar *path = NULL;
    g_autoptr (GFile) dir_location = NULL;
    g_autoptr (GFile) home_location = g_file_new_for_path (g_get_home_dir ());
    g_autoptr (GFile) root_location = g_file_new_for_path ("/");
    GFile *relative_location_base;

    if (path_attribute_q == 0)
    {
        return NULL;
    }

    path = nautilus_file_get_string_attribute_q (file, path_attribute_q);
    dir_location = g_file_new_for_commandline_arg (path);

    if (base_location != NULL && g_file_equal (base_location, dir_location))
    {
        /* Only occurs when search result is
         * a direct child of the base location
         */
        return NULL;
    }

    if (g_file_equal (dir_location, home_location))
    {
        return nautilus_compute_title_for_location (home_location);
    }

    relative_location_base = base_location;
    if (relative_location_base == NULL)
    {
        /* Only occurs in Recent, Starred and Trash. */
        relative_location_base = home_location;
    }

    if (!g_file_equal (relative_location_base, root_location) &&
        g_file_has_prefix (dir_location, relative_location_base))
    {
        g_autofree gchar *relative_path = NULL;
        g_autofree gchar *display_name = NULL;

        relative_path = g_file_get_relative_path (relative_location_base, dir_location);
        display_name = g_filename_display_name (relative_path);

        /* Ensure a trailing slash to emphasize it is a directory */
        if (g_str_has_suffix (display_name, G_DIR_SEPARATOR_S))
        {
            return g_steal_pointer (&display_name);
        }

        return g_strconcat (display_name, G_DIR_SEPARATOR_S, NULL);
    }

    return g_steal_pointer (&path);
}

static void
update_labels (NautilusNameCell *self)
{
    NautilusViewItem *item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    NautilusFile *file;
    g_autofree gchar *path_text = NULL;
    const gchar *fts_snippet = NULL;

    g_return_if_fail (item != NULL);
    file = nautilus_view_item_get_file (item);

    path_text = get_path_text (file,
                               self->path_attribute_q,
                               self->file_path_base_location);
    if (self->show_snippet)
    {
        fts_snippet = nautilus_file_get_search_fts_snippet (file);
    }

    gtk_label_set_text (GTK_LABEL (self->label),
                        nautilus_file_get_display_name (file));
    gtk_label_set_text (GTK_LABEL (self->path), path_text);
    gtk_label_set_markup (GTK_LABEL (self->snippet), fts_snippet);

    gtk_widget_set_visible (self->path, (path_text != NULL));
    gtk_widget_set_visible (self->snippet_button, (fts_snippet != NULL));
}

static void
update_icon (NautilusNameCell *self)
{
    NautilusFileIconFlags flags;
    g_autoptr (GdkPaintable) icon_paintable = NULL;
    NautilusViewItem *item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    NautilusFile *file;
    guint icon_size;
    gint scale_factor;
    int icon_height;
    int extra_margin;

    g_return_if_fail (item != NULL);

    file = nautilus_view_item_get_file (item);
    g_object_get (self, "icon-size", &icon_size, NULL);
    scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));
    flags = NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS;

    icon_paintable = nautilus_file_get_icon_paintable (file, icon_size, scale_factor, flags);
    gtk_picture_set_paintable (GTK_PICTURE (self->icon), icon_paintable);

    /* Set the same width for all icons regardless of aspect ratio.
     * Don't set the width here because it would get GtkPicture w4h confused.
     */
    gtk_widget_set_size_request (self->fixed_height_box, icon_size, -1);

    /* Give all items the same minimum width. This cannot be done by setting the
     * width request directly, as above, because it would get mess up with
     * height for width calculations.
     *
     * Instead we must add margins on both sides of the icon which, summed up
     * with the icon's actual width, equal the desired item width. */
    icon_height = gdk_paintable_get_intrinsic_height (icon_paintable);
    extra_margin = (icon_size - icon_height) / 2;
    gtk_widget_set_margin_top (self->fixed_height_box, extra_margin);
    gtk_widget_set_margin_bottom (self->fixed_height_box, extra_margin);

    if (icon_size >= NAUTILUS_THUMBNAIL_MINIMUM_ICON_SIZE &&
        nautilus_file_get_thumbnail_path (file) != NULL &&
        nautilus_file_should_show_thumbnail (file) &&
        nautilus_file_check_if_ready (file, NAUTILUS_FILE_ATTRIBUTE_THUMBNAIL))
    {
        gtk_widget_add_css_class (self->icon, "thumbnail");
    }
    else
    {
        gtk_widget_remove_css_class (self->icon, "thumbnail");
    }
}

static void
update_emblems (NautilusNameCell *self)
{
    NautilusViewItem *item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    NautilusFile *file;
    GtkWidget *child;
    GtkIconTheme *theme;
    g_autolist (GIcon) emblems = NULL;

    g_return_if_fail (item != NULL);
    file = nautilus_view_item_get_file (item);

    /* Remove old emblems. */
    while ((child = gtk_widget_get_first_child (self->emblems_box)) != NULL)
    {
        gtk_box_remove (GTK_BOX (self->emblems_box), child);
    }

    theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
    emblems = nautilus_file_get_emblem_icons (file);
    for (GList *l = emblems; l != NULL; l = l->next)
    {
        if (!gtk_icon_theme_has_gicon (theme, l->data))
        {
            g_autofree gchar *icon_string = g_icon_to_string (l->data);
            g_warning ("Failed to add emblem. “%s” not found in the icon theme",
                       icon_string);
            continue;
        }

        gtk_box_append (GTK_BOX (self->emblems_box),
                        gtk_image_new_from_gicon (l->data));
    }
}

static void
on_file_changed (NautilusNameCell *self)
{
    update_icon (self);
    update_labels (self);
    update_emblems (self);
}

static void
on_icon_size_changed (NautilusNameCell *self)
{
    NautilusViewItem *item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));

    if (item == NULL)
    {
        /* Cell is not bound to an item yet. Do nothing. */
        return;
    }

    update_icon (self);
}

static void
on_item_drag_accept_changed (NautilusNameCell *self)
{
    gboolean drag_accept;
    NautilusViewItem *item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    GtkWidget *list_row = gtk_widget_get_parent (gtk_widget_get_parent (GTK_WIDGET (self)));

    g_object_get (item, "drag-accept", &drag_accept, NULL);
    if (drag_accept)
    {
        gtk_widget_set_state_flags (list_row, GTK_STATE_FLAG_DROP_ACTIVE, FALSE);
    }
    else
    {
        gtk_widget_unset_state_flags (list_row, GTK_STATE_FLAG_DROP_ACTIVE);
    }
}

static void
on_item_is_cut_changed (NautilusNameCell *self)
{
    gboolean is_cut;
    NautilusViewItem *item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));

    g_object_get (item,
                  "is-cut", &is_cut,
                  NULL);
    if (is_cut)
    {
        gtk_widget_add_css_class (self->icon, "cut");
    }
    else
    {
        gtk_widget_remove_css_class (self->icon, "cut");
    }
}

static gboolean
on_loading_timeout (gpointer user_data)
{
    NautilusNameCell *self = NAUTILUS_NAME_CELL (user_data);
    NautilusViewItem *item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    gboolean is_loading;

    self->loading_timeout_id = 0;

    g_object_get (item,
                  "is-loading", &is_loading,
                  NULL);
    if (is_loading)
    {
        gtk_widget_set_visible (self->spinner, TRUE);
        gtk_spinner_start (GTK_SPINNER (self->spinner));
    }

    return G_SOURCE_REMOVE;
}

static void
on_item_is_loading_changed (NautilusNameCell *self)
{
    gboolean is_loading;
    NautilusViewItem *item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));


    g_clear_handle_id (&self->loading_timeout_id, g_source_remove);
    g_object_get (item, "is-loading", &is_loading, NULL);
    if (is_loading)
    {
        self->loading_timeout_id = g_timeout_add_seconds (LOADING_TIMEOUT_SECONDS,
                                                          G_SOURCE_FUNC (on_loading_timeout),
                                                          self);
    }
    else
    {
        gtk_widget_set_visible (self->spinner, FALSE);
        gtk_spinner_stop (GTK_SPINNER (self->spinner));
    }
}

static void
nautilus_name_cell_init (NautilusNameCell *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    g_signal_connect (self, "notify::icon-size",
                      G_CALLBACK (on_icon_size_changed), NULL);

    /* Connect automatically to an item. */
    self->item_signal_group = g_signal_group_new (NAUTILUS_TYPE_VIEW_ITEM);
    g_signal_group_connect_swapped (self->item_signal_group, "notify::drag-accept",
                                    (GCallback) on_item_drag_accept_changed, self);
    g_signal_group_connect_swapped (self->item_signal_group, "notify::is-cut",
                                    (GCallback) on_item_is_cut_changed, self);
    g_signal_group_connect_swapped (self->item_signal_group, "notify::is-loading",
                                    (GCallback) on_item_is_loading_changed, self);
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
nautilus_name_cell_dispose (GObject *object)
{
    NautilusNameCell *self = (NautilusNameCell *) object;

    gtk_widget_dispose_template (GTK_WIDGET (self), NAUTILUS_TYPE_NAME_CELL);

    G_OBJECT_CLASS (nautilus_name_cell_parent_class)->dispose (object);
}

static void
nautilus_name_cell_finalize (GObject *object)
{
    NautilusNameCell *self = (NautilusNameCell *) object;

    g_clear_handle_id (&self->loading_timeout_id, g_source_remove);
    g_clear_object (&self->item_signal_group);
    g_clear_object (&self->file_path_base_location);
    G_OBJECT_CLASS (nautilus_name_cell_parent_class)->finalize (object);
}

static void
nautilus_name_cell_class_init (NautilusNameCellClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = nautilus_name_cell_dispose;
    object_class->finalize = nautilus_name_cell_finalize;

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-name-cell.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusNameCell, expander);
    gtk_widget_class_bind_template_child (widget_class, NautilusNameCell, content);
    gtk_widget_class_bind_template_child (widget_class, NautilusNameCell, fixed_height_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusNameCell, spinner);
    gtk_widget_class_bind_template_child (widget_class, NautilusNameCell, icon);
    gtk_widget_class_bind_template_child (widget_class, NautilusNameCell, label);
    gtk_widget_class_bind_template_child (widget_class, NautilusNameCell, emblems_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusNameCell, snippet_button);
    gtk_widget_class_bind_template_child (widget_class, NautilusNameCell, snippet);
    gtk_widget_class_bind_template_child (widget_class, NautilusNameCell, path);
}

NautilusViewCell *
nautilus_name_cell_new (NautilusListBase *view)
{
    return NAUTILUS_VIEW_CELL (g_object_new (NAUTILUS_TYPE_NAME_CELL,
                                             "view", view,
                                             NULL));
}

void
nautilus_name_cell_set_path (NautilusNameCell *self,
                             GQuark            path_attribute_q,
                             GFile            *base_location)
{
    self->path_attribute_q = path_attribute_q;
    g_set_object (&self->file_path_base_location, base_location);
}

void
nautilus_name_cell_show_snippet (NautilusNameCell *self)
{
    self->show_snippet = TRUE;
}

GtkTreeExpander *
nautilus_name_cell_get_expander (NautilusNameCell *self)
{
    return GTK_TREE_EXPANDER (self->expander);
}

GtkWidget *
nautilus_name_cell_get_content (NautilusNameCell *self)
{
    return self->content;
}
