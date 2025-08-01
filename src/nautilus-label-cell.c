/*
 * Copyright (C) 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-label-cell.h"

/* Needed for NautilusColumn (full GType). */
#include <nautilus-extension.h>

#include "nautilus-column.h"
#include "nautilus-file.h"
#include "nautilus-global-preferences.h"
#include "nautilus-view-item.h"

struct _NautilusLabelCell
{
    NautilusViewCell parent_instance;

    GSignalGroup *item_signal_group;

    GQuark attribute_q;
    gulong date_format_handler_id;

    GtkLabel *label;

    gboolean show_snippet;
};

G_DEFINE_TYPE (NautilusLabelCell, nautilus_label_cell, NAUTILUS_TYPE_VIEW_CELL)

static void
update_label (NautilusLabelCell *self)
{
    g_autoptr (NautilusViewItem) item = NULL;
    NautilusFile *file;
    g_autofree gchar *string = NULL;

    item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    g_return_if_fail (item != NULL);
    file = nautilus_view_item_get_file (item);

    string = nautilus_file_get_string_attribute_q (file, self->attribute_q);
    gtk_label_set_text (self->label, string);
}

static void
nautilus_label_cell_init (NautilusLabelCell *self)
{
    GtkWidget *child;

    child = gtk_label_new (NULL);
    gtk_widget_set_parent (child, GTK_WIDGET (self));
    gtk_widget_set_valign (child, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class (child, "dim-label");
    self->label = GTK_LABEL (child);

    /* Connect automatically to an item. */
    self->item_signal_group = g_signal_group_new (NAUTILUS_TYPE_VIEW_ITEM);
    g_signal_group_connect_swapped (self->item_signal_group, "file-changed",
                                    G_CALLBACK (update_label), self);
    g_signal_connect_object (self->item_signal_group, "bind",
                             G_CALLBACK (update_label), self,
                             G_CONNECT_SWAPPED);
    g_object_bind_property (self, "item",
                            self->item_signal_group, "target",
                            G_BINDING_SYNC_CREATE);
}

static void
nautilus_label_cell_dispose (GObject *object)
{
    NautilusLabelCell *self = (NautilusLabelCell *) object;

    g_clear_object (&self->item_signal_group);
    if (self->label)
    {
        gtk_widget_unparent (GTK_WIDGET (self->label));
        self->label = NULL;
    }

    g_clear_signal_handler (&self->date_format_handler_id, nautilus_preferences);

    G_OBJECT_CLASS (nautilus_label_cell_parent_class)->dispose (object);
}

static void
nautilus_label_cell_class_init (NautilusLabelCellClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = nautilus_label_cell_dispose;

    gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
nautilus_label_cell_setup_for_column (NautilusLabelCell *self,
                                      NautilusColumn    *column)
{
    g_autofree gchar *column_name = NULL;
    gfloat xalign;

    g_object_get (column,
                  "attribute_q", &self->attribute_q,
                  "name", &column_name,
                  "xalign", &xalign,
                  NULL);
    gtk_label_set_xalign (self->label, xalign);

    if (nautilus_file_is_date_sort_attribute_q (self->attribute_q))
    {
        self->date_format_handler_id =
            g_signal_connect_swapped (nautilus_preferences,
                                      "changed::" NAUTILUS_PREFERENCES_DATE_TIME_FORMAT,
                                      G_CALLBACK (update_label), self);
    }

    if (g_strcmp0 (column_name, "permissions") == 0)
    {
        gtk_widget_add_css_class (GTK_WIDGET (self->label), "monospace");
        gtk_widget_remove_css_class (GTK_WIDGET (self->label), "numeric");
    }
    else
    {
        gtk_widget_add_css_class (GTK_WIDGET (self->label), "numeric");
        gtk_widget_remove_css_class (GTK_WIDGET (self->label), "monospace");
    }
}

#define CACHED_CELLS_INIT_COUNT 200
#define CACHED_CELLS_MAX_COUNT 1650
static GPtrArray *cached_cells;

void
nautilus_label_cell_recycle (NautilusLabelCell **self)
{
    g_return_if_fail (NAUTILUS_IS_LABEL_CELL (*self));

    g_clear_signal_handler (&(*self)->date_format_handler_id, nautilus_preferences);

    if (!g_ptr_array_find (cached_cells, *self, NULL) &&
        cached_cells->len < CACHED_CELLS_MAX_COUNT)
    {
        g_object_set (*self, "item", NULL, NULL);

        g_ptr_array_add (cached_cells, g_steal_pointer (self));
    }

    g_clear_object (self);
}

static void
ensure_cells (void)
{
    if (cached_cells == NULL)
    {
        cached_cells = g_ptr_array_new_with_free_func (g_object_unref);

        for (uint i = 0; i < CACHED_CELLS_INIT_COUNT; i++)
        {
            NautilusLabelCell *cell = g_object_new (NAUTILUS_TYPE_LABEL_CELL, NULL);

            g_ptr_array_add (cached_cells, g_object_ref_sink (cell));
        }
    }
}

void
nautilus_label_cell_clear_cache (void)
{
    g_clear_pointer (&cached_cells, g_ptr_array_unref);
}

NautilusViewCell *
nautilus_label_cell_new (NautilusColumn *column)
{
    NautilusLabelCell *cell;

    ensure_cells ();

    if (cached_cells->len == 0)
    {
        cell = g_object_new (NAUTILUS_TYPE_LABEL_CELL, NULL);

        /* Needed to avoid warnings when clearing the cache. */
        g_object_ref_sink (cell);
    }
    else
    {
        cell = g_ptr_array_steal_index (cached_cells, cached_cells->len - 1);

        g_assert (gtk_widget_get_parent (GTK_WIDGET (cell)) == NULL);
    }

    nautilus_label_cell_setup_for_column (cell, column);

    return NAUTILUS_VIEW_CELL (cell);
}
