/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-grid-cell.h"

struct _NautilusGridCell
{
    NautilusViewCell parent_instance;

    GSignalGroup *item_signal_group;

    GQuark *caption_attributes;

    GtkWidget *fixed_height_box;
    GtkWidget *icon;
    GtkWidget *emblems_box;
    GtkWidget *label;
    GtkWidget *first_caption;
    GtkWidget *second_caption;
    GtkWidget *third_caption;
};

G_DEFINE_TYPE (NautilusGridCell, nautilus_grid_cell, NAUTILUS_TYPE_VIEW_CELL)

static void
update_icon (NautilusGridCell *self)
{
    g_autoptr (NautilusViewItem) item = NULL;
    NautilusFileIconFlags flags;
    g_autoptr (GdkPaintable) icon_paintable = NULL;
    GtkStyleContext *style_context;
    NautilusFile *file;
    guint icon_size;
    gint scale_factor;
    g_autofree gchar *thumbnail_path = NULL;

    item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    g_return_if_fail (item != NULL);
    file = nautilus_view_item_get_file (item);
    icon_size = nautilus_view_item_get_icon_size (item);
    scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));
    flags = NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS;

    icon_paintable = nautilus_file_get_icon_paintable (file, icon_size, scale_factor, flags);
    gtk_picture_set_paintable (GTK_PICTURE (self->icon), icon_paintable);

    /* Set the same height and width for all icons regardless of aspect ratio.
     */
    gtk_widget_set_size_request (self->fixed_height_box, icon_size, icon_size);
    style_context = gtk_widget_get_style_context (self->icon);
    thumbnail_path = nautilus_file_get_thumbnail_path (file);
    if (thumbnail_path != NULL &&
        nautilus_file_should_show_thumbnail (file))
    {
        gtk_style_context_add_class (style_context, "thumbnail");
    }
    else
    {
        gtk_style_context_remove_class (style_context, "thumbnail");
    }
}

static void
update_captions (NautilusGridCell *self)
{
    g_autoptr (NautilusViewItem) item = NULL;
    NautilusFile *file;
    GtkWidget * const caption_labels[] =
    {
        self->first_caption,
        self->second_caption,
        self->third_caption
    };
    G_STATIC_ASSERT (G_N_ELEMENTS (caption_labels) == NAUTILUS_GRID_CELL_N_CAPTIONS);

    item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    g_return_if_fail (item != NULL);
    file = nautilus_view_item_get_file (item);
    for (guint i = 0; i < NAUTILUS_GRID_CELL_N_CAPTIONS; i++)
    {
        GQuark attribute_q = self->caption_attributes[i];
        gboolean show_caption;

        show_caption = (attribute_q != 0);
        gtk_widget_set_visible (caption_labels[i], show_caption);
        if (show_caption)
        {
            g_autofree gchar *string = NULL;
            string = nautilus_file_get_string_attribute_q (file, attribute_q);
            gtk_label_set_text (GTK_LABEL (caption_labels[i]), string);
        }
    }
}

static void
update_emblems (NautilusGridCell *self)
{
    g_autoptr (NautilusViewItem) item = NULL;
    NautilusFile *file;
    GtkWidget *child;
    GtkIconTheme *theme;
    g_autolist (GIcon) emblems = NULL;

    item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
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
        g_autoptr (GtkIconPaintable) icon_paintable = NULL;

        /* Workaround for gtk_icon_theme_has_gicon() ignoring unthemed icons.
         * See: https://gitlab.gnome.org/GNOME/nautilus/-/issues/2789 and
         * https://gitlab.gnome.org/GNOME/gtk/-/issues/5709 */
        icon_paintable = gtk_icon_theme_lookup_by_gicon (theme, l->data, 16, 1, 0, 0);
        if (g_strcmp0 (gtk_icon_paintable_get_icon_name (icon_paintable), "image-missing") == 0)
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
on_file_changed (NautilusGridCell *self)
{
    g_autoptr (NautilusViewItem) item = NULL;
    NautilusFile *file;
    g_autofree gchar *name = NULL;

    item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    g_return_if_fail (item != NULL);
    file = nautilus_view_item_get_file (item);

    update_icon (self);
    update_emblems (self);

    name = nautilus_file_get_display_name (file);

    gtk_label_set_text (GTK_LABEL (self->label), name);
    update_captions (self);
}

static void
on_item_size_changed (NautilusGridCell *self)
{
    update_icon (self);
    update_captions (self);
}

static void
on_item_is_cut_changed (NautilusGridCell *self)
{
    gboolean is_cut;
    g_autoptr (NautilusViewItem) item = NULL;

    item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
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
on_label_query_tooltip (GtkWidget  *widget,
                        int         x,
                        int         y,
                        gboolean    keyboard_tip,
                        GtkTooltip *tooltip,
                        gpointer    user_data)
{
    GtkLabel *label = GTK_LABEL (widget);

    if (pango_layout_is_ellipsized (gtk_label_get_layout (label)))
    {
        gtk_tooltip_set_text (tooltip, gtk_label_get_text (label));
        return TRUE;
    }

    return FALSE;
}

static void
finalize (GObject *object)
{
    NautilusGridCell *self = (NautilusGridCell *) object;

    g_object_unref (self->item_signal_group);
    G_OBJECT_CLASS (nautilus_grid_cell_parent_class)->finalize (object);
}

static void
nautilus_grid_cell_class_init (NautilusGridCellClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->finalize = finalize;

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-grid-cell.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusGridCell, fixed_height_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusGridCell, icon);
    gtk_widget_class_bind_template_child (widget_class, NautilusGridCell, emblems_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusGridCell, label);
    gtk_widget_class_bind_template_child (widget_class, NautilusGridCell, first_caption);
    gtk_widget_class_bind_template_child (widget_class, NautilusGridCell, second_caption);
    gtk_widget_class_bind_template_child (widget_class, NautilusGridCell, third_caption);

    gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GRID_CELL);
}

static void
nautilus_grid_cell_init (NautilusGridCell *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    g_signal_connect (self->label, "query-tooltip",
                      G_CALLBACK (on_label_query_tooltip), NULL);

    /* Connect automatically to an item. */
    self->item_signal_group = g_signal_group_new (NAUTILUS_TYPE_VIEW_ITEM);
    g_signal_group_connect_swapped (self->item_signal_group, "notify::icon-size",
                                    (GCallback) on_item_size_changed, self);
    g_signal_group_connect_swapped (self->item_signal_group, "notify::is-cut",
                                    (GCallback) on_item_is_cut_changed, self);
    g_signal_group_connect_swapped (self->item_signal_group, "file-changed",
                                    (GCallback) on_file_changed, self);
    g_signal_connect_object (self->item_signal_group, "bind",
                             (GCallback) on_file_changed, self,
                             G_CONNECT_SWAPPED);

    g_object_bind_property (self, "item",
                            self->item_signal_group, "target",
                            G_BINDING_SYNC_CREATE);

#if PANGO_VERSION_CHECK (1, 44, 4)
    {
        PangoAttrList *attr_list;

        /* GTK4 TODO: This attribute is set in the UI file but GTK 3 ignores it.
         * Remove this block after the switch to GTK 4. */
        attr_list = pango_attr_list_new ();
        pango_attr_list_insert (attr_list, pango_attr_insert_hyphens_new (FALSE));
        gtk_label_set_attributes (GTK_LABEL (self->label), attr_list);
        pango_attr_list_unref (attr_list);
    }
#endif
}

NautilusGridCell *
nautilus_grid_cell_new (NautilusListBase *view)
{
    return g_object_new (NAUTILUS_TYPE_GRID_CELL,
                         "view", view,
                         NULL);
}

void
nautilus_grid_cell_set_caption_attributes (NautilusGridCell *self,
                                           GQuark           *attrs)
{
    self->caption_attributes = attrs;
}
