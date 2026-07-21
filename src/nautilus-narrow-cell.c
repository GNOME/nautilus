/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-narrow-cell.h"

#include "nautilus-file.h"
#include "nautilus-global-preferences.h"
#include "nautilus-image.h"
#include "nautilus-star-cell.h"
#include "nautilus-thumbnails.h"
#include "nautilus-ui-utilities.h"
#include "nautilus-view-item.h"
#include "nautilus-view-cell.h"

struct _NautilusNarrowCell
{
    NautilusViewCell parent_instance;

    GSignalGroup *item_signal_group;

    GQuark *caption_attributes;

    GtkWidget *top_child;
    GtkWidget *icon;
    GtkWidget *emblems_box;
    GtkWidget *captions_box;
    GtkWidget *date_label;
    GtkWidget *size_label;

    gboolean in_file_change;
};

G_DEFINE_TYPE (NautilusNarrowCell, nautilus_narrow_cell, NAUTILUS_TYPE_VIEW_CELL)

static void
update_icon (NautilusNarrowCell *self)
{
    g_autoptr (NautilusViewItem) item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));

    g_return_if_fail (item != NULL);

    gboolean is_cut;
    g_object_get (item, "is-cut", &is_cut, NULL);
    if (is_cut)
    {
        gtk_widget_set_visible (self->icon, FALSE);
        gtk_widget_remove_css_class (self->icon, "hidden-file");

        return;
    }

    guint icon_size;
    g_object_get (self, "icon-size", &icon_size, NULL);

    NautilusFile *file = nautilus_view_item_get_file (item);
    gint scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (self));
    NautilusFileIconFlags flags = NAUTILUS_FILE_ICON_FLAGS_NONE;
    g_autoptr (GdkPaintable) icon_paintable = nautilus_file_get_icon_paintable (file, icon_size, scale_factor, flags);

    gtk_widget_set_visible (self->icon, TRUE);
    nautilus_image_set_size (NAUTILUS_IMAGE (self->icon), icon_size);
    nautilus_image_set_fallback (NAUTILUS_IMAGE (self->icon), icon_paintable);

    gboolean show_thumbnail = nautilus_file_should_show_thumbnail (file);

    if (self->in_file_change ||
        !show_thumbnail)
    {
        nautilus_image_set_source (NAUTILUS_IMAGE (self->icon), NULL);
    }

    if (show_thumbnail)
    {
        g_autoptr (GFile) location = nautilus_file_get_location (file);

        nautilus_image_set_source (NAUTILUS_IMAGE (self->icon), location);
    }

    if (nautilus_file_is_hidden_file (file))
    {
        gtk_widget_add_css_class (self->icon, "hidden-file");
    }
    else
    {
        gtk_widget_remove_css_class (self->icon, "hidden-file");
    }
}

static void
update_captions (NautilusNarrowCell *self)
{
    g_autoptr (NautilusViewItem) item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    g_return_if_fail (item != NULL);
    NautilusFile *file = nautilus_view_item_get_file (item);

    g_autofree gchar *date_string = nautilus_file_get_string_attribute (file, "date_modified");
    gtk_label_set_text (GTK_LABEL (self->date_label), date_string);

    g_autofree gchar *size_string = nautilus_file_get_string_attribute (file, "size");
    gtk_label_set_text (GTK_LABEL (self->size_label), size_string);

    guint icon_size;
    g_object_get (self, "icon-size", &icon_size, NULL);

    gtk_widget_set_visible (self->captions_box,
                            (icon_size >= NAUTILUS_LIST_ICON_SIZE_MEDIUM));
}

static void
update_emblems (NautilusNarrowCell *self)
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
    file_uri = nautilus_file_get_activation_uri (file);

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
on_file_changed (NautilusNarrowCell *self)
{
    self->in_file_change = TRUE;

    update_icon (self);
    update_emblems (self);
    update_captions (self);

    self->in_file_change = FALSE;
}

static void
on_icon_size_changed (NautilusNarrowCell *self)
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
nautilus_narrow_cell_dispose (GObject *object)
{
    NautilusNarrowCell *self = (NautilusNarrowCell *) object;

    gtk_widget_dispose_template (GTK_WIDGET (self), NAUTILUS_TYPE_NARROW_CELL);
    g_clear_object (&self->item_signal_group);

    G_OBJECT_CLASS (nautilus_narrow_cell_parent_class)->dispose (object);
}

static void
snapshot (GtkWidget   *widget,
          GtkSnapshot *snapshot)
{
    NautilusNarrowCell *self = NAUTILUS_NARROW_CELL (widget);
    g_autoptr (NautilusViewItem) item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    gboolean is_cut;

    g_object_get (item, "is-cut", &is_cut, NULL);

    if (is_cut)
    {
        graphene_rect_t icon_bounds;

        if (!gtk_widget_compute_bounds (self->icon, widget, &icon_bounds))
        {
            g_warning ("Could not compute icon bounds in cell coordinates.");
        }
        else
        {
            AdwStyleManager *style_manager = adw_style_manager_get_default ();
            gboolean is_high_contrast = adw_style_manager_get_high_contrast (style_manager);
            const double border_opacity = is_high_contrast ? 0.5 : 0.15;
            const double dim_opacity = is_high_contrast ? 0.9 : 0.55;
            guint icon_size;

            g_object_get (self, "icon-size", &icon_size, NULL);

            const gchar *resource = (icon_size <= NAUTILUS_LIST_ICON_SIZE_MEDIUM ?
                                     "/org/gnome/nautilus/icons/scalable/actions/cut-symbolic.svg" :
                                     "/org/gnome/nautilus/icons/scalable/actions/cut-large-symbolic.svg");
            GdkRGBA color;
            gtk_widget_get_color (widget, &color);

            if (icon_size >= NAUTILUS_THUMBNAIL_MINIMUM_ICON_SIZE)
            {
                GdkRGBA dashed_border_color = color;
                graphene_rect_t dash_bounds = icon_bounds;

                dashed_border_color.alpha *= border_opacity;
                nautilus_ui_draw_icon_dashed_border (snapshot, &dash_bounds, dashed_border_color);

                graphene_rect_inset_r (&dash_bounds,
                                       0.2 * dash_bounds.size.width,
                                       0.2 * dash_bounds.size.height,
                                       &icon_bounds);
            }

            GdkRGBA icon_color = color;
            icon_color.alpha *= dim_opacity;

            g_autoptr (GtkSvg) svg = gtk_svg_new_from_resource (resource);
            nautilus_ui_draw_svg (snapshot, svg, &icon_bounds, icon_color);
        }
    }

    GTK_WIDGET_CLASS (nautilus_narrow_cell_parent_class)->snapshot (widget, snapshot);
}

static void
nautilus_narrow_cell_class_init (NautilusNarrowCellClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = nautilus_narrow_cell_dispose;

    widget_class->snapshot = snapshot;

    g_type_ensure (NAUTILUS_TYPE_IMAGE);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-narrow-cell.ui");
    gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

    /* Needs to add the direct child of the template widget to dispose it since
     * a plain GtkWidget doesn't dispose it's child automatically. */
    gtk_widget_class_bind_template_child (widget_class, NautilusNarrowCell, top_child);
    gtk_widget_class_bind_template_child (widget_class, NautilusNarrowCell, icon);
    gtk_widget_class_bind_template_child (widget_class, NautilusNarrowCell, emblems_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusNarrowCell, captions_box);
    gtk_widget_class_bind_template_child (widget_class, NautilusNarrowCell, date_label);
    gtk_widget_class_bind_template_child (widget_class, NautilusNarrowCell, size_label);

    gtk_widget_class_bind_template_callback (widget_class, on_label_query_tooltip);

    gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_NONE);
}

static void
nautilus_narrow_cell_init (NautilusNarrowCell *self)
{
    g_type_ensure (NAUTILUS_TYPE_STAR_CELL);
    gtk_widget_init_template (GTK_WIDGET (self));

    g_signal_connect (self, "notify::icon-size",
                      G_CALLBACK (on_icon_size_changed), NULL);
    g_signal_connect (self, "notify::scale-factor", G_CALLBACK (on_icon_size_changed), NULL);

    g_signal_connect_object (nautilus_preferences, "changed::" NAUTILUS_PREFERENCES_DATE_TIME_FORMAT,
                             G_CALLBACK (update_captions), self,
                             G_CONNECT_SWAPPED);

    /* Connect automatically to an item. */
    self->item_signal_group = g_signal_group_new (NAUTILUS_TYPE_VIEW_ITEM);
    g_signal_group_connect_swapped (self->item_signal_group, "notify::is-cut",
                                    (GCallback) update_icon, self);
    g_signal_group_connect_swapped (self->item_signal_group, "file-changed",
                                    (GCallback) on_file_changed, self);
    g_signal_connect_object (self->item_signal_group, "bind",
                             (GCallback) on_file_changed, self,
                             G_CONNECT_SWAPPED);

    g_object_bind_property (self, "item",
                            self->item_signal_group, "target",
                            G_BINDING_SYNC_CREATE);
}

NautilusNarrowCell *
nautilus_narrow_cell_new (NautilusListBase *view)
{
    return g_object_new (NAUTILUS_TYPE_NARROW_CELL,
                         "view", view,
                         NULL);
}
