/*
 * Copyright (C) 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* Needed for NautilusColumn (full GType). */
#include <nautilus-extension.h>

#include "nautilus-label-cell.h"

struct _NautilusLabelCell
{
    NautilusViewCell parent_instance;

    GSignalGroup *item_signal_group;

    NautilusColumn *column;
    GQuark attribute_q;

    GtkLabel *label;

    gboolean show_snippet;
};

G_DEFINE_TYPE (NautilusLabelCell, nautilus_label_cell, NAUTILUS_TYPE_VIEW_CELL)

enum
{
    PROP_0,
    PROP_COLUMN,
    N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
on_file_changed (NautilusLabelCell *self)
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
nautilus_label_cell_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
    NautilusLabelCell *self = NAUTILUS_LABEL_CELL (object);

    switch (prop_id)
    {
        case PROP_COLUMN:
        {
            self->column = g_value_get_object (value);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_label_cell_init (NautilusLabelCell *self)
{
    GtkWidget *child;

    child = gtk_label_new (NULL);
    adw_bin_set_child (ADW_BIN (self), child);
    gtk_widget_set_valign (child, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class (child, "dim-label");
    self->label = GTK_LABEL (child);

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
nautilus_label_cell_constructed (GObject *object)
{
    NautilusLabelCell *self = NAUTILUS_LABEL_CELL (object);
    g_autofree gchar *column_name = NULL;
    gfloat xalign;

    G_OBJECT_CLASS (nautilus_label_cell_parent_class)->constructed (object);

    g_object_get (self->column,
                  "attribute_q", &self->attribute_q,
                  "name", &column_name,
                  "xalign", &xalign,
                  NULL);
    gtk_label_set_xalign (self->label, xalign);

    if (g_strcmp0 (column_name, "permissions") == 0)
    {
        gtk_widget_add_css_class (GTK_WIDGET (self->label), "monospace");
    }
    else
    {
        gtk_widget_add_css_class (GTK_WIDGET (self->label), "numeric");
    }
}

static void
nautilus_label_cell_finalize (GObject *object)
{
    NautilusLabelCell *self = (NautilusLabelCell *) object;

    g_object_unref (self->item_signal_group);
    G_OBJECT_CLASS (nautilus_label_cell_parent_class)->finalize (object);
}

static void
nautilus_label_cell_class_init (NautilusLabelCellClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->set_property = nautilus_label_cell_set_property;
    object_class->constructed = nautilus_label_cell_constructed;
    object_class->finalize = nautilus_label_cell_finalize;

    properties[PROP_COLUMN] = g_param_spec_object ("column",
                                                   "", "",
                                                   NAUTILUS_TYPE_COLUMN,
                                                   G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

NautilusViewCell *
nautilus_label_cell_new (NautilusListBase *view,
                         NautilusColumn   *column)
{
    return NAUTILUS_VIEW_CELL (g_object_new (NAUTILUS_TYPE_LABEL_CELL,
                                             "view", view,
                                             "column", column,
                                             NULL));
}
