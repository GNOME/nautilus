/*
 * Copyright © 2024 The Files contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 */

#include "nautilus-minimal-cell.h"

#include <glib.h>
#include <gtk/gtk.h>

struct _NautilusMinimalCell
{
    GObject parent_instance;

    gchar *title;
    gchar *subtitle;

    GObject *paintable;
};

G_DEFINE_TYPE (NautilusMinimalCell, nautilus_minimal_cell, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_PAINTABLE,
    PROP_SUBTITLE,
    PROP_TITLE,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

const gchar *
nautilus_minimal_cell_get_subtitle (NautilusMinimalCell *self)
{
    return self->subtitle;
}

const gchar *
nautilus_minimal_cell_get_title (NautilusMinimalCell *self)
{
    return self->title;
}

static void
nautilus_minimal_cell_get_property (GObject    *object,
                                    guint       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
    NautilusMinimalCell *self = NAUTILUS_MINIMAL_CELL (object);

    switch (property_id)
    {
        case PROP_PAINTABLE:
        {
            g_value_set_object (value, self->paintable);
        }
        break;

        case PROP_SUBTITLE:
        {
            g_value_set_string (value, self->subtitle);
        }
        break;

        case PROP_TITLE:
        {
            g_value_set_string (value, self->title);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

static void
nautilus_minimal_cell_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
    NautilusMinimalCell *self = NAUTILUS_MINIMAL_CELL (object);

    switch (property_id)
    {
        case PROP_PAINTABLE:
        {
            self->paintable = g_value_get_object (value);
        }
        break;

        case PROP_SUBTITLE:
        {
            g_set_str (&self->subtitle, g_value_get_string (value));
        }
        break;

        case PROP_TITLE:
        {
            g_set_str (&self->title, g_value_get_string (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

static void
nautilus_minimal_cell_init (NautilusMinimalCell *self)
{
}

static void
nautilus_minimal_cell_dispose (GObject *object)
{
    NautilusMinimalCell *self = NAUTILUS_MINIMAL_CELL (object);

    g_clear_object (&self->paintable);
    g_clear_pointer (&self->subtitle, g_free);
    g_clear_pointer (&self->title, g_free);

    G_OBJECT_CLASS (nautilus_minimal_cell_parent_class)->dispose (object);
}

static void
nautilus_minimal_cell_class_init (NautilusMinimalCellClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->get_property = nautilus_minimal_cell_get_property;
    object_class->set_property = nautilus_minimal_cell_set_property;
    object_class->dispose = nautilus_minimal_cell_dispose;

    properties[PROP_PAINTABLE] = g_param_spec_object ("paintable", NULL, NULL,
                                                      G_TYPE_OBJECT,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_STRINGS);

    properties[PROP_SUBTITLE] = g_param_spec_string ("subtitle", NULL, NULL,
                                                     NULL,
                                                     G_PARAM_READWRITE |
                                                     G_PARAM_CONSTRUCT_ONLY |
                                                     G_PARAM_STATIC_STRINGS);

    properties[PROP_TITLE] = g_param_spec_string ("title", NULL, NULL,
                                                  NULL,
                                                  G_PARAM_READWRITE |
                                                  G_PARAM_CONSTRUCT_ONLY |
                                                  G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

NautilusMinimalCell *
nautilus_minimal_cell_new (gchar        *title,
                           gchar        *subtitle,
                           GdkPaintable *paintable)
{
    return NAUTILUS_MINIMAL_CELL (g_object_new (NAUTILUS_TYPE_MINIMAL_CELL,
                                                "title", title,
                                                "subtitle", subtitle,
                                                "paintable", paintable,
                                                NULL));
}
