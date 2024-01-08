/*
 * Copyright (C) 2022 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-view-cell.h"
#include "nautilus-list-base.h"

/**
 * NautilusViewCell:
 *
 * Abstract class of widgets tailored to be set as #GtkListItem:child in a view
 * which subclasses #NautilusListBase.
 *
 * Subclass constructors should take a pointer to the #NautilusListBase view.
 *
 * The view is responsible for setting #NautilusViewCell:item. This can be done
 * using a GBinding from #GtkListItem:item to #NautilusViewCell:item.
 */

typedef struct _NautilusViewCellPrivate NautilusViewCellPrivate;
struct _NautilusViewCellPrivate
{
    AdwBin parent_instance;

    NautilusListBase *view; /* Unowned */
    NautilusViewItem *item; /* Owned reference */

    guint icon_size;
    guint position;

    gboolean called_once;
};


G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (NautilusViewCell, nautilus_view_cell, ADW_TYPE_BIN)

enum
{
    PROP_0,
    PROP_VIEW,
    PROP_ITEM,
    PROP_ICON_SIZE,
    PROP_POSITION,
    N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
nautilus_view_cell_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
    NautilusViewCell *self = NAUTILUS_VIEW_CELL (object);
    NautilusViewCellPrivate *priv = nautilus_view_cell_get_instance_private (self);

    switch (prop_id)
    {
        case PROP_VIEW:
        {
            g_value_set_object (value, priv->view);
        }
        break;

        case PROP_ITEM:
        {
            g_value_set_object (value, priv->item);
        }
        break;

        case PROP_ICON_SIZE:
        {
            g_value_set_uint (value, priv->icon_size);
        }
        break;

        case PROP_POSITION:
        {
            g_value_set_uint (value, priv->position);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_view_cell_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    NautilusViewCell *self = NAUTILUS_VIEW_CELL (object);
    NautilusViewCellPrivate *priv = nautilus_view_cell_get_instance_private (self);

    switch (prop_id)
    {
        case PROP_VIEW:
        {
            g_set_weak_pointer (&priv->view, g_value_get_object (value));
        }
        break;

        case PROP_ITEM:
        {
            g_set_object (&priv->item, g_value_get_object (value));
        }
        break;

        case PROP_ICON_SIZE:
        {
            priv->icon_size = g_value_get_uint (value);
        }
        break;

        case PROP_POSITION:
        {
            priv->position = g_value_get_uint (value);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_view_cell_init (NautilusViewCell *self)
{
    gtk_widget_set_name (GTK_WIDGET (self), "NautilusViewCell");
}

static void
nautilus_view_cell_finalize (GObject *object)
{
    NautilusViewCell *self = NAUTILUS_VIEW_CELL (object);
    NautilusViewCellPrivate *priv = nautilus_view_cell_get_instance_private (self);

    g_clear_object (&priv->item);
    g_clear_weak_pointer (&priv->view);

    G_OBJECT_CLASS (nautilus_view_cell_parent_class)->finalize (object);
}

static void
nautilus_view_cell_class_init (NautilusViewCellClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = nautilus_view_cell_finalize;
    object_class->get_property = nautilus_view_cell_get_property;
    object_class->set_property = nautilus_view_cell_set_property;

    properties[PROP_VIEW] = g_param_spec_object ("view",
                                                 "", "",
                                                 NAUTILUS_TYPE_LIST_BASE,
                                                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    properties[PROP_ITEM] = g_param_spec_object ("item",
                                                 "", "",
                                                 NAUTILUS_TYPE_VIEW_ITEM,
                                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    properties[PROP_ICON_SIZE] = g_param_spec_uint ("icon-size",
                                                    "", "",
                                                    NAUTILUS_LIST_ICON_SIZE_SMALL,
                                                    NAUTILUS_GRID_ICON_SIZE_EXTRA_LARGE,
                                                    NAUTILUS_GRID_ICON_SIZE_LARGE,
                                                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_POSITION] = g_param_spec_uint ("position", NULL, NULL,
                                                   0, G_MAXUINT, GTK_INVALID_LIST_POSITION,
                                                   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (object_class, N_PROPS, properties);
}

gboolean
nautilus_view_cell_once (NautilusViewCell *self)
{
    NautilusViewCellPrivate *priv = nautilus_view_cell_get_instance_private (self);

    if (priv->called_once)
    {
        return FALSE;
    }
    priv->called_once = TRUE;

    return TRUE;
}

guint
nautilus_view_cell_get_position (NautilusViewCell *self)
{
    NautilusViewCellPrivate *priv = nautilus_view_cell_get_instance_private (self);

    return priv->position;
}

NautilusListBase *
nautilus_view_cell_get_view (NautilusViewCell *self)
{
    g_return_val_if_fail (NAUTILUS_IS_VIEW_CELL (self), NULL);

    NautilusViewCellPrivate *priv = nautilus_view_cell_get_instance_private (self);

    return priv->view;
}

void
nautilus_view_cell_set_item (NautilusViewCell *self,
                             NautilusViewItem *item)
{
    g_return_if_fail (NAUTILUS_IS_VIEW_CELL (self));
    g_return_if_fail (item == NULL || NAUTILUS_IS_VIEW_ITEM (item));

    g_object_set (self, "item", item, NULL);
}

NautilusViewItem *
nautilus_view_cell_get_item (NautilusViewCell *self)
{
    NautilusViewItem *item;

    g_return_val_if_fail (NAUTILUS_IS_VIEW_CELL (self), NULL);

    /* This gets a full reference. */
    g_object_get (self, "item", &item, NULL);

    /* Return full reference for consistency with gtk_tree_list_row_get_item() */
    return item;
}
