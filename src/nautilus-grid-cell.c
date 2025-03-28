/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-grid-cell.h"

#include "nautilus-global-preferences.h"
#include "nautilus-tag-manager.h"
#include "nautilus-thumbnails.h"

struct _NautilusGridCell
{
    NautilusViewCell parent_instance;

    GSignalGroup *item_signal_group;

    GQuark *caption_attributes;

    GtkWidget *icon;
    GtkWidget *emblems_box;
    GtkWidget *labels_box;
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
    NautilusFile *file;
    guint icon_size;
    gint scale_factor;

    item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    g_return_if_fail (item != NULL);
    file = nautilus_view_item_get_file (item);
    g_object_get (self, "icon-size", &icon_size, NULL);
    scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));
    flags = NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS;

    icon_paintable = nautilus_file_get_icon_paintable (file, icon_size, scale_factor, flags);
    gtk_picture_set_paintable (GTK_PICTURE (self->icon), icon_paintable);

    if (nautilus_file_has_thumbnail (file) &&
        nautilus_file_should_show_thumbnail (file))
    {
        gtk_widget_add_css_class (self->icon, "thumbnail");
    }
    else
    {
        gtk_widget_remove_css_class (self->icon, "thumbnail");
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
    g_autofree gchar *file_uri = NULL;

    item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    g_return_if_fail (item != NULL);
    file = nautilus_view_item_get_file (item);
    file_uri = nautilus_file_get_uri (file);

    /* Remove old emblems. */
    while ((child = gtk_widget_get_first_child (self->emblems_box)) != NULL)
    {
        gtk_box_remove (GTK_BOX (self->emblems_box), child);
    }

    if (nautilus_tag_manager_file_is_starred (nautilus_tag_manager_get (), file_uri))
    {
        gtk_box_append (GTK_BOX (self->emblems_box),
                        gtk_image_new_from_icon_name ("starred-symbolic"));
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
on_file_changed (NautilusGridCell *self)
{
    update_icon (self);
    update_emblems (self);
    update_captions (self);
}

static void
on_icon_size_changed (NautilusGridCell *self)
{
    g_autoptr (NautilusViewItem) item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));

    if (item == NULL)
    {
        /* Cell is not bound to an item yet. Do nothing. */
        return;
    }

    update_icon (self);
    update_captions (self);
    gtk_widget_queue_resize (GTK_WIDGET (self));
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
on_starred_changed (NautilusTagManager *tag_manager,
                    GList              *changed_files,
                    gpointer            user_data)
{
    NautilusGridCell *self = NAUTILUS_GRID_CELL (user_data);
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
        update_emblems (self);
    }
}

static void
on_map_changed (GtkWidget *widget,
                gpointer   user_data)
{
    NautilusViewCell *cell = NAUTILUS_VIEW_CELL (widget);
    gboolean is_mapped = GPOINTER_TO_INT (user_data);
    g_autoptr (NautilusViewItem) item = nautilus_view_cell_get_item (cell);

    g_return_if_fail (item != NULL);

    NautilusFile *file = nautilus_view_item_get_file (item);

    if (nautilus_file_is_thumbnailing (file) ||
        !nautilus_file_check_if_ready (file, NAUTILUS_FILE_ATTRIBUTE_THUMBNAIL_INFO) ||
        !nautilus_file_check_if_ready (file, NAUTILUS_FILE_ATTRIBUTE_THUMBNAIL_BUFFER))
    {
        nautilus_view_item_prioritize (item, is_mapped);
    }
}

static void
nautilus_grid_cell_dispose (GObject *object)
{
    NautilusGridCell *self = (NautilusGridCell *) object;

    gtk_widget_dispose_template (GTK_WIDGET (self), NAUTILUS_TYPE_GRID_CELL);
    g_clear_object (&self->item_signal_group);

    G_OBJECT_CLASS (nautilus_grid_cell_parent_class)->dispose (object);
}

#define EMBLEMS_BOX_WIDTH 18
#define VERTICAL_PADDING 6

static void
nautilus_grid_cell_measure (GtkWidget      *widget,
                            GtkOrientation  orientation,
                            int             for_size,
                            int            *minimum,
                            int            *natural,
                            int            *min_baseline,
                            int            *nat_baseline)
{
    NautilusGridCell *self = NAUTILUS_GRID_CELL (widget);
    guint icon_size;
    int width, child_min, child_nat;

    g_object_get (self, "icon-size", &icon_size, NULL);
    width = EMBLEMS_BOX_WIDTH + icon_size + EMBLEMS_BOX_WIDTH;

    /* We always expect our layout to fit into this width. Check that
     * this is indeed the case. This also shuts up the
     *     Allocating size to ... without calling gtk_widget_measure().
     *     How does the code know the size to allocate?
     * warning that we get in debug builds of GTK otherwise. The answer
     * to that question is: we know that the icon is a GtkPicture, and
     * we set the icon_paintable of the specific size on it.
     */
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        gtk_widget_measure (self->labels_box, orientation, -1,
                            &child_min, NULL, NULL, NULL);
        if (G_UNLIKELY (child_min > width))
        {
            g_warning ("%s %p unexpectedly doesn't fit into width of %d",
                       gtk_widget_get_name (self->labels_box), self->labels_box, width);
            width = child_min;
        }
        gtk_widget_measure (self->icon, orientation, -1,
                            &child_min, NULL, NULL, NULL);
        if (G_UNLIKELY (child_min > (int) icon_size))
        {
            g_warning ("%s %p unexpectedly doesn't fit into width of %d",
                       gtk_widget_get_name (self->icon), self->icon, icon_size);
            width += child_min - icon_size;
        }
        gtk_widget_measure (self->emblems_box, orientation, -1,
                            &child_min, NULL, NULL, NULL);
        if (G_UNLIKELY (child_min > EMBLEMS_BOX_WIDTH))
        {
            g_warning ("%s %p unexpectedly doesn't fit into width of %d",
                       gtk_widget_get_name (self->emblems_box), self->emblems_box, EMBLEMS_BOX_WIDTH);
            width += 2 * (child_min - EMBLEMS_BOX_WIDTH);
        }
    }
    else /* GTK_ORIENTATION_VERTICAL */
    {
        gtk_widget_measure (self->icon, orientation, icon_size,
                            &child_min, NULL, NULL, NULL);
        if (G_UNLIKELY (child_min > (int) icon_size))
        {
            g_warning ("%s %p unexpectedly doesn't fit into height of %d",
                       gtk_widget_get_name (self->icon), self->icon, icon_size);
        }
        gtk_widget_measure (self->emblems_box, orientation, EMBLEMS_BOX_WIDTH,
                            &child_min, NULL, NULL, NULL);
        if (G_UNLIKELY (child_min > (int) icon_size))
        {
            g_warning ("%s %p unexpectedly doesn't fit into height of %d",
                       gtk_widget_get_name (self->emblems_box), self->emblems_box, icon_size);
        }
    }

    if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        *natural = *minimum = width;
        *min_baseline = *nat_baseline = -1;
        return;
    }

    gtk_widget_measure (self->labels_box, GTK_ORIENTATION_VERTICAL, width,
                        &child_min, &child_nat,
                        min_baseline, nat_baseline);

    *minimum = icon_size + VERTICAL_PADDING + child_min;
    *natural = icon_size + VERTICAL_PADDING + child_nat;
    if (*min_baseline != -1)
    {
        *min_baseline += icon_size + VERTICAL_PADDING;
    }
    if (*nat_baseline != -1)
    {
        *nat_baseline += icon_size + VERTICAL_PADDING;
    }
}

static void
nautilus_grid_cell_size_allocate (GtkWidget *widget,
                                  int        width,
                                  int        height,
                                  int        baseline)
{
    NautilusGridCell *self = NAUTILUS_GRID_CELL (widget);
    GtkAllocation child_allocation;
    guint icon_size;

    g_object_get (self, "icon-size", &icon_size, NULL);

    /* Put the icon at the top. Center it horizontally, with
     * EMBLEMS_BOX_WIDTH horizontal margins on both sides.
     * Note that the icon is expected to center itself further
     * inside the allocation that we give it.
     */
    child_allocation = (GtkAllocation) {
        EMBLEMS_BOX_WIDTH, 0,
        width - EMBLEMS_BOX_WIDTH * 2, icon_size
    };
    gtk_widget_size_allocate (self->icon, &child_allocation, -1);

    /* Put the emblems_box into the "end" margin. */
    child_allocation.width = EMBLEMS_BOX_WIDTH;
    if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    {
        child_allocation.x = width - EMBLEMS_BOX_WIDTH;
    }
    else
    {
        child_allocation.x = 0;
    }
    gtk_widget_size_allocate (self->emblems_box, &child_allocation, -1);

    /* Give the remaining space to labels box. */
    child_allocation = (GtkAllocation) {
        0, icon_size + VERTICAL_PADDING,
        width, height - (icon_size + VERTICAL_PADDING)
    };
    if (baseline != -1)
    {
        baseline -= icon_size + VERTICAL_PADDING;
    }
    gtk_widget_size_allocate (self->labels_box, &child_allocation, baseline);
}

static void
nautilus_grid_cell_class_init (NautilusGridCellClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = nautilus_grid_cell_dispose;

    widget_class->measure = nautilus_grid_cell_measure;
    widget_class->size_allocate = nautilus_grid_cell_size_allocate;

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-grid-cell.ui");

    gtk_widget_class_bind_template_child (widget_class, NautilusGridCell, icon);
    gtk_widget_class_bind_template_child (widget_class, NautilusGridCell, emblems_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusGridCell, labels_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusGridCell, first_caption);
    gtk_widget_class_bind_template_child (widget_class, NautilusGridCell, second_caption);
    gtk_widget_class_bind_template_child (widget_class, NautilusGridCell, third_caption);

    gtk_widget_class_bind_template_callback (widget_class, on_label_query_tooltip);

    gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GRID_CELL);
}

static void
nautilus_grid_cell_init (NautilusGridCell *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    g_signal_connect (self, "map", G_CALLBACK (on_map_changed), GINT_TO_POINTER (TRUE));
    g_signal_connect (self, "unmap", G_CALLBACK (on_map_changed), GINT_TO_POINTER (FALSE));
    g_signal_connect (self, "notify::icon-size",
                      G_CALLBACK (on_icon_size_changed), NULL);

    g_signal_connect_object (nautilus_tag_manager_get (), "starred-changed",
                             G_CALLBACK (on_starred_changed), self, G_CONNECT_DEFAULT);

    g_signal_connect_object (nautilus_preferences, "changed::" NAUTILUS_PREFERENCES_DATE_TIME_FORMAT,
                             G_CALLBACK (update_captions), self,
                             G_CONNECT_SWAPPED);

    /* Connect automatically to an item. */
    self->item_signal_group = g_signal_group_new (NAUTILUS_TYPE_VIEW_ITEM);
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
