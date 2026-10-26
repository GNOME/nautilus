/*
 * SPDX-FileCopyrightText: 2022 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>
#include "nautilus-properties-item.h"

/**
 * NautilusPropertiesItem:
 *
 * A single element in a file's properties.
 *
 * A file's properties will consist of one or more `NautilusPropertiesItem`.
 * Each item is a name/value pair.  Items are added to their corresponding
 * [property@NautilusPropertiesModel:model].
 */

enum
{
    PROP_NAME = 1,
    PROP_VALUE,
    NUM_PROPS
};

static GParamSpec *properties[NUM_PROPS];

struct _NautilusPropertiesItem
{
    GObject parent_instance;

    char *name;
    char *value;
};

G_DEFINE_FINAL_TYPE (NautilusPropertiesItem, nautilus_properties_item, G_TYPE_OBJECT)

/**
 * nautilus_properties_item_new:
 * @name: the user-visible name for the properties item.
 * @value: the user-visible value for the properties item.
 *
 * Create a new `NautilusPropertiesItem`
 *
 * returns: (transfer full): a new `NautilusPropertiesItem`
 */
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
    GObjectClass *object_class = G_OBJECT_CLASS (class);

    object_class->finalize = nautilus_properties_item_finalize;
    object_class->get_property = nautilus_properties_item_get_property;
    object_class->set_property = nautilus_properties_item_set_property;

    /**
     * NautilusPropertiesItem:name:
     *
     * The user-visible name.
     */
    properties[PROP_NAME] =
        g_param_spec_string ("name", NULL, NULL,
                             NULL,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /**
     * NautilusPropertiesItem:value:
     *
     * The user-visible value.
     */
    properties[PROP_VALUE] =
        g_param_spec_string ("value", NULL, NULL,
                             NULL,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, NUM_PROPS, properties);
}

/**
 * nautilus_properties_item_get_name:
 *
 * Get the name.
 *
 * Returns: (transfer none): the name of this `NautilusPropertiesItem`
 */
const char *
nautilus_properties_item_get_name (NautilusPropertiesItem *self)
{
    return self->name;
}

/**
 * nautilus_properties_item_get_value:
 *
 * Get the value.
 *
 * Returns: (transfer none): the value of this `NautilusPropertiesItem`
 */
const char *
nautilus_properties_item_get_value (NautilusPropertiesItem *self)
{
    return self->value;
}
