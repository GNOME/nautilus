/*
 * Copyright (C) 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>
#include "nautilus-properties-item.h"

enum
{
    PROP_0,
    PROP_NAME,
    PROP_VALUE,
    LAST_PROP
};

struct _NautilusPropertiesItem
{
    GObject parent_instance;

    char *name;
    char *value;
};

G_DEFINE_TYPE (NautilusPropertiesItem, nautilus_properties_item, G_TYPE_OBJECT)

NautilusPropertiesItem *
nautilus_properties_item_new (const char *name,
                              const char *value)
{
    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (name != NULL, NULL);

    return g_object_new (NAUTILUS_TYPE_PROPERTIES_ITEM,
                         "name", name,
                         "value", value,
                         NULL);
}

static void
nautilus_properties_item_get_property (GObject    *object,
                                       guint       param_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
    NautilusPropertiesItem *self = NAUTILUS_PROPERTIES_ITEM (object);

    switch (param_id)
    {
        case PROP_NAME:
        {
            g_value_set_string (value, self->name);
        }
        break;

        case PROP_VALUE:
        {
            g_value_set_string (value, self->value);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        }
        break;
    }
}

static void
nautilus_properties_item_set_property (GObject      *object,
                                       guint         param_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
    NautilusPropertiesItem *self = NAUTILUS_PROPERTIES_ITEM (object);

    switch (param_id)
    {
        case PROP_NAME:
        {
            g_free (self->name);
            self->name = g_value_dup_string (value);
        }
        break;

        case PROP_VALUE:
        {
            g_free (self->value);
            self->value = g_value_dup_string (value);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        }
        break;
    }
}

static void
nautilus_properties_item_finalize (GObject *object)
{
    NautilusPropertiesItem *self = NAUTILUS_PROPERTIES_ITEM (object);

    g_free (self->name);
    g_free (self->value);

    G_OBJECT_CLASS (nautilus_properties_item_parent_class)->finalize (object);
}

static void
nautilus_properties_item_init (NautilusPropertiesItem *self)
{
}

static void
nautilus_properties_item_class_init (NautilusPropertiesItemClass *class)
{
    GParamSpec *pspec;

    G_OBJECT_CLASS (class)->finalize = nautilus_properties_item_finalize;
    G_OBJECT_CLASS (class)->get_property = nautilus_properties_item_get_property;
    G_OBJECT_CLASS (class)->set_property = nautilus_properties_item_set_property;

    pspec = g_param_spec_string ("name", "", "",
                                 NULL,
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property (G_OBJECT_CLASS (class), PROP_NAME, pspec);

    pspec = g_param_spec_string ("value", "", "",
                                 NULL,
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property (G_OBJECT_CLASS (class), PROP_VALUE, pspec);
}

const char *
nautilus_properties_item_get_name (NautilusPropertiesItem *self)
{
    return self->name;
}

const char *
nautilus_properties_item_get_value (NautilusPropertiesItem *self)
{
    return self->value;
}
